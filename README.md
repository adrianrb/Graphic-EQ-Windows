# Graphic EQ — Windows

Visualizador de espectro da saída padrão do Windows, port nativo do
[`graph-eq`](../graph-eq) Android para **C++17 + Win32 + GDI + WASAPI loopback**.

Versão atual: **1.1.0**.

<img src="icon128x128.png" alt="Ícone do Graphic EQ" width="96">

## O que faz

- Analisa o áudio que está tocando no dispositivo de saída padrão (loopback),
  sem tocar ou modificar o som.
- FFT de 2048 amostras com janela Hann, recalculada a cada 10 ms.
- 3 a 19 bandas logarítmicas entre 25 Hz e 16 kHz.
- Escala de −72 a −6 dB com tilt de +2,5 dB/oitava, como no app Android.
- LEDs com ataque de 35 ms, queda de 100 ms, pico com hold de 350 ms e
  tema semáforo opcional.
- Painel sem moldura, sempre visível, arrastável e com opacidade ajustável
  de 0% a 100% no fundo (LEDs, texto e borda continuam opacos).
- Opções de sensibilidade, largura, cor dos LEDs, tema, pico, topmost e
  inicialização com o Windows.
- Ícone na bandeja, instância única e configuração persistida em
  `GraphicEQ.ini` ao lado do `.exe`.
- Um único `.exe` estático: sem DLLs de runtime do MinGW, rede ou telemetria.

## Importante

Este programa é um **visualizador**, não um equalizador de processamento. Ele
apenas lê a saída padrão do Windows via WASAPI loopback. O áudio reproduzido não
é alterado.

O loopback só recebe dados enquanto há reprodução. Com o player pausado, o painel
fica em silêncio (comportamento esperado). Aplicativos que usam modo exclusivo ou
ASIO podem não ser capturados pelo loopback compartilhado.

## Uso

- **Arraste** o painel com o botão esquerdo para movê-lo.
- **Duplo clique** abre as opções.
- **Clique direito** abre o menu: opções, tamanho, sempre visível, sobre e sair.
- O **ícone da bandeja** abre o mesmo menu.
- `Alt+F4` encerra.

As opções são aplicadas na hora. O botão **Salvar** grava o `.ini`; fechar pelo
`X` apenas esconde a janela de opções. O programa também salva ao arrastar,
redimensionar ou sair.

## Build

Requisitos no Linux/WSL:

- `x86_64-w64-mingw32-g++` (GCC 12 ou mais novo)
- `x86_64-w64-mingw32-windres`
- `make`

```sh
make release  # release -> output/GraphicEQ.exe
make          # alias para o build release
make test     # roda os testes de DSP no host
make clean    # apaga build/ e output/
```

O build padrão é release: `-O2 -DNDEBUG` e link com `-s` (sem símbolos de
depuração). O `Makefile` usa `-static`; sem isso o executável dependeria de
`libstdc++-6.dll`, `libgcc_s_seh-1.dll` e `libwinpthread-1.dll`. Para conferir:

```sh
x86_64-w64-mingw32-objdump -p output/GraphicEQ.exe | grep "DLL Name"
```

Devem aparecer apenas DLLs do sistema (KERNEL32, USER32, GDI32, OLE32,
COMCTL32, COMdlg32, ADVAPI32, SHELL32, msvcrt).

O ícone é gerado por `tools/make_icon.py` (requer Pillow) e já está versionado em
`src/res/app.ico`; o build não precisa de Python.

## Estrutura

```
src/main.cpp              entrada, instância única, Alt+F4
src/app/                  ids, estado global, configuração e versão
src/audio/capture.*       WASAPI loopback do dispositivo padrão
src/dsp/                  FFT, plano de bandas e análise espectral
src/gfx/render.*          desenho GDI e janela em camadas
src/ui/                   janela principal, opções, menu e bandeja
src/res/                  ícone, metadados e IDs de recurso
tests/dsp_test.cpp        teste de host (FFT/bandas/escala)
tools/make_icon.py        gerador do ícone
```

## Configuração

`GraphicEQ.ini` fica ao lado do executável e usa a seção `[EQ]`:

| Chave | Padrão | Significado |
| --- | --- | --- |
| `X`, `Y` | canto superior direito | posição do painel |
| `W` | `520` | largura em pixels (260–1300) |
| `Bands` | `15` | número de bandas (3–19) |
| `Sensitivity` | `50` | sensibilidade (0–100) |
| `Opacity` | `85` | opacidade do fundo do painel (0–100%) |
| `Color` | `16770304` | cor base dos LEDs (`COLORREF` em decimal; padrão `0x00FFE500`) |
| `Traffic` | `0` | tema semáforo (0/1) |
| `Peak` | `1` | exibir pico (0/1) |
| `Topmost` | `1` | sempre visível (0/1) |
| `Startup` | `0` | iniciar com o Windows (0/1) |

O autostart usa `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`.

## Versão

`version.txt` é a fonte da verdade. A versão também aparece em
`src/app/version.h` (`APP_VERSION`) e nos metadados de `src/res/app.rc`; o
`make` falha se os três divergirem.

## Referências

- [`../graph-eq`](../graph-eq) — comportamento canônico de DSP e UI no Android.
- [`../VU-LED-Windows`](../VU-LED-Windows) — esqueleto Win32/WASAPI e gotchas de
  GDI usados como base.
