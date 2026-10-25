// FFT radix-2 em ponto flutuante (mesmo algoritmo do graph-eq Android).
#pragma once
#include <vector>

namespace dsp {

class Fft {
public:
    // Transforma in-place; re/im precisam ter o mesmo tamanho e ser potência de 2.
    static void transform(std::vector<float>& re, std::vector<float>& im);
};

}  // namespace dsp
