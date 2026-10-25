#include "audio/capture.h"

#include <algorithm>
#include <cmath>
#include <cstring>

// O ksmedia.h do MinGW-w64 não fornece esta definição.
static const GUID GUID_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT = {
    0x00000003, 0x0000, 0x0010,
    {0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71}
};

AudioCapture gAudio;

void AudioCapture::Shutdown() {
    if (capture)    { capture->Release();    capture = nullptr; }
    if (client)     { client->Release();     client = nullptr; }
    if (mix)        { CoTaskMemFree(mix);    mix = nullptr; }
    if (device)     { device->Release();     device = nullptr; }
    if (enumerator) { enumerator->Release(); enumerator = nullptr; }
    ready = false;
    channels = 0;
    blockAlign = 0;
    format = Format::Unsupported;
}

bool AudioCapture::Init() {
    Shutdown();

    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
                                  CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
                                  reinterpret_cast<void**>(&enumerator));
    if (FAILED(hr)) {
        return false;
    }

    hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
    if (FAILED(hr)) {
        Shutdown();
        return false;
    }

    hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr,
                          reinterpret_cast<void**>(&client));
    if (FAILED(hr)) {
        Shutdown();
        return false;
    }

    hr = client->GetMixFormat(&mix);
    if (FAILED(hr) || !mix) {
        Shutdown();
        return false;
    }

    channels = mix->nChannels;
    sampleRate = static_cast<int>(mix->nSamplesPerSec);
    blockAlign = mix->nBlockAlign;

    if (mix->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        const auto* ext = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(mix);
        if (IsEqualGUID(ext->SubFormat, GUID_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)) {
            format = Format::Float32;
        } else {
            switch (mix->wBitsPerSample) {
            case 16: format = Format::Int16; break;
            case 24: format = Format::Int24; break;
            case 32: format = Format::Int32; break;
            default: format = Format::Unsupported; break;
            }
        }
    } else if (mix->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) {
        format = Format::Float32;
    } else if (mix->wFormatTag == WAVE_FORMAT_PCM) {
        switch (mix->wBitsPerSample) {
        case 16: format = Format::Int16; break;
        case 24: format = Format::Int24; break;
        case 32: format = Format::Int32; break;
        default: format = Format::Unsupported; break;
        }
    } else {
        format = Format::Unsupported;
    }

    if (format == Format::Unsupported || channels <= 0 || blockAlign <= 0) {
        Shutdown();
        return false;
    }

    hr = client->Initialize(AUDCLNT_SHAREMODE_SHARED,
                            AUDCLNT_STREAMFLAGS_LOOPBACK,
                            0, 0, mix, nullptr);
    if (FAILED(hr)) {
        Shutdown();
        return false;
    }

    hr = client->GetService(__uuidof(IAudioCaptureClient),
                            reinterpret_cast<void**>(&capture));
    if (FAILED(hr)) {
        Shutdown();
        return false;
    }

    hr = client->Start();
    if (FAILED(hr)) {
        Shutdown();
        return false;
    }

    ready = true;
    return true;
}

std::size_t AudioCapture::Poll(std::vector<float>& mono) {
    mono.clear();

    if (!ready) {
        const ULONGLONG now = GetTickCount64();
        if (now >= retryAt) {
            retryAt = now + 1000;
            Init();
        }
        return 0;
    }

    std::size_t produced = 0;
    for (;;) {
        BYTE* data = nullptr;
        UINT32 frames = 0;
        DWORD flags = 0;
        const HRESULT hr =
            capture->GetBuffer(&data, &frames, &flags, nullptr, nullptr);
        if (hr == AUDCLNT_S_BUFFER_EMPTY) {
            break;
        }
        if (FAILED(hr)) {
            // Dispositivo removido/trocado, formato inválido etc.
            Shutdown();
            retryAt = GetTickCount64() + 1000;
            return produced;
        }

        if (frames == 0) {
            capture->ReleaseBuffer(0);
            break;
        }

        {
            const bool silent = (flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0;
            if (silent || !data) {
                if (mono.size() < 96000) {
                    mono.insert(mono.end(), frames, 0.0f);
                    produced += frames;
                }
            } else if (mono.size() < 96000) {
                const BYTE* p = data;
                for (UINT32 f = 0; f < frames; ++f) {
                    float soma = 0.0f;
                    for (int ch = 0; ch < channels; ++ch) {
                        switch (format) {
                        case Format::Float32: {
                            float v;
                            std::memcpy(&v, p, sizeof(float));
                            soma += v;
                            p += sizeof(float);
                            break;
                        }
                        case Format::Int16: {
                            int16_t v;
                            std::memcpy(&v, p, sizeof(int16_t));
                            soma += static_cast<float>(v) / 32768.0f;
                            p += sizeof(int16_t);
                            break;
                        }
                        case Format::Int24: {
                            int32_t v = static_cast<int32_t>(p[0]) |
                                        (static_cast<int32_t>(p[1]) << 8) |
                                        (static_cast<int32_t>(p[2]) << 16);
                            if (v & 0x00800000) {
                                v |= static_cast<int32_t>(0xFF000000);
                            }
                            soma += static_cast<float>(v) / 8388608.0f;
                            p += 3;
                            break;
                        }
                        case Format::Int32: {
                            int32_t v;
                            std::memcpy(&v, p, sizeof(int32_t));
                            soma += static_cast<float>(
                                        static_cast<double>(v) /
                                        2147483648.0);
                            p += sizeof(int32_t);
                            break;
                        }
                        case Format::Unsupported:
                            break;
                        }
                    }
                    const float m = std::clamp(
                        soma / static_cast<float>(channels), -1.0f, 1.0f);
                    mono.push_back(m);
                    ++produced;
                }
            }
            capture->ReleaseBuffer(frames);
        }
    }
    return produced;
}
