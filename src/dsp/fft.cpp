#include "dsp/fft.h"

#include <cmath>
#include <stdexcept>
#include <utility>

namespace dsp {

void Fft::transform(std::vector<float>& re, std::vector<float>& im) {
    const int n = static_cast<int>(re.size());
    if (re.size() != im.size() || n <= 0 || (n & (n - 1)) != 0) {
        throw std::invalid_argument("tamanho de FFT invalido");
    }

    for (int i = 1, j = 0; i < n; ++i) {
        int bit = n >> 1;
        for (; (j & bit) != 0; bit >>= 1) {
            j ^= bit;
        }
        j ^= bit;
        if (i < j) {
            std::swap(re[i], re[j]);
            std::swap(im[i], im[j]);
        }
    }

    for (int len = 2; len <= n; len <<= 1) {
        const double angulo = -2.0 * 3.14159265358979323846 / len;
        const float passoR = static_cast<float>(std::cos(angulo));
        const float passoI = static_cast<float>(std::sin(angulo));
        const int meio = len >> 1;
        for (int i = 0; i < n; i += len) {
            float wr = 1.0f;
            float wi = 0.0f;
            for (int j = 0; j < meio; ++j) {
                const int a = i + j;
                const int b = a + meio;
                const float vr = re[b] * wr - im[b] * wi;
                const float vi = re[b] * wi + im[b] * wr;
                re[b] = re[a] - vr;
                im[b] = im[a] - vi;
                re[a] += vr;
                im[a] += vi;
                const float nwr = wr * passoR - wi * passoI;
                wi = wr * passoI + wi * passoR;
                wr = nwr;
            }
        }
    }
}

}  // namespace dsp
