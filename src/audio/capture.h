// Captura WASAPI loopback do dispositivo de saída padrão.
// Entrega amostras mono normalizadas (-1..1) para a análise espectral.
#pragma once
#include <windows.h>
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <mmreg.h>

#include <cstddef>
#include <cstdint>
#include <vector>

struct AudioCapture {
    IMMDeviceEnumerator* enumerator = nullptr;
    IMMDevice*           device     = nullptr;
    IAudioClient*        client     = nullptr;
    IAudioCaptureClient* capture    = nullptr;
    WAVEFORMATEX*        mix        = nullptr;

    enum class Format { Float32, Int16, Int24, Int32, Unsupported };

    Format format = Format::Unsupported;
    int    channels = 0;
    int    sampleRate = 48000;
    int    blockAlign = 0;
    bool   ready = false;
    ULONGLONG retryAt = 0;

    void Shutdown();
    bool Init();
    // Drena todos os pacotes disponíveis; `mono` recebe as amostras mono.
    // Retorna a quantidade de amostras produzidas (0 se não há áudio/dados).
    std::size_t Poll(std::vector<float>& mono);

    bool Ready() const { return ready; }
    int SampleRate() const { return sampleRate; }
};

extern AudioCapture gAudio;
