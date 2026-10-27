# AGENTS.md

## Current state

- Native Windows port of `../graph-eq` at **v1.1.0**: C++17 + Win32 + GDI + WASAPI
  loopback, cross-compiled with MinGW-w64 to one portable `.exe`.
- This is a **visualizer, not an audio-processing equalizer**. It only reads the default
  output via WASAPI loopback and must never modify or render audio.
- Android-only plumbing (MediaProjection, overlay permission, foreground service, boot
  receiver) is intentionally not ported. The Windows equivalents are a frameless topmost
  panel, tray icon/menu, `GraphicEQ.ini`, and `HKCU\...\Run`.
- No CI yet. `make` and `make test` are the real entrypoints; do not invent others.

## Build and verify

```sh
make release         # release -> output/GraphicEQ.exe (-O2 -DNDEBUG, -s)
make                 # alias para release
make test            # compila e roda o teste de DSP no host (g++)
make clean           # apaga build/ e output/
```

- Toolchain: `x86_64-w64-mingw32-g++` (GCC 12), `x86_64-w64-mingw32-windres`, `make`.
- Keep the checkout under `/mnt/c/...`, not `/home/...`; Windows tooling must see it.
- `-static` is mandatory. Without it the `.exe` depends on `libstdc++-6.dll`,
  `libgcc_s_seh-1.dll`, and `libwinpthread-1.dll`, absent on stock Windows. Verify with:
  `x86_64-w64-mingw32-objdump -p output/GraphicEQ.exe | grep "DLL Name"` — system DLLs only.
- `make test` has no Win32 dependency; it validates FFT, band mapping, dB normalization,
  silence decay, and labels. Manual GUI/WASAPI smoke testing must be done on Windows.
- `build/`, `output/`, `*.ini`, and test captures are ignored.

## Layout

```
src/main.cpp                     entrada, instância única, acelerador Alt+F4
src/app/ids.h                    constantes, IDs, classes de janela
src/app/state.{h,cpp}            preferências, níveis e handles globais
src/app/config.{h,cpp}           GraphicEQ.ini + autostart no registro
src/app/version.h                APP_VERSION (sincronizado com version.txt)
src/audio/capture.{h,cpp}        WASAPI loopback -> amostras mono
src/dsp/fft.{h,cpp}              FFT radix-2 in-place
src/dsp/band_plan.{h,cpp}        3–19 bandas log, 25 Hz–16 kHz, rótulos
src/dsp/spectrum.{h,cpp}         janela Hann, hop de 10 ms, escala dB/tilt
src/gfx/render.{h,cpp}           painel GDI, paleta de LEDs, DIB + layered alpha
src/ui/main_window.{h,cpp}       timer de áudio, suavização, arrasto, comandos
src/ui/options_window.{h,cpp}    janela de opções (controles criados em código)
src/ui/menu.{h,cpp}              menu de contexto
src/ui/tray.{h,cpp}              ícone da bandeja
src/res/                         app.ico, app.rc, resource.h
tests/dsp_test.cpp               teste de host do DSP
tools/make_icon.py               gerador do ícone (Pillow; não faz parte do build)
```

## DSP / UI invariants (do not drift from graph-eq)

- Bandas: `3..19`, logarítmicas entre `25 Hz` e `16 kHz`. Bordas são médias geométricas;
  a primeira usa `max(20, c0*sqrt(c0/c1))`, a última `cN*sqrt(cN/cN-1)`.
- FFT de `2048` amostras com janela Hann; hop = `round(sampleRate/100)` (10 ms).
  Magnitude `4*hypot(re,im)/N`; pico por banda; `20*log10(pico)`; tilt de
  `+2.5 dB/oitava` relativo a 1 kHz; normalização `-72..-6 dB`.
- Níveis exibidos: ataque `35 ms`, queda `100 ms`, `dt` real do timer; pico com hold de
  `350 ms` e queda de `0.55/s`. Abaixo de `0.002` o nível zera.
- Sensibilidade `0..100` mapeia ganho `0.45..1.55` (`sensitivity/100*1.1+0.45`) no nível
  normalizado, depois do DSP.
- Tema semáforo: verde `<60%`, amarelo `<85%`, laranja `<90%`, vermelho `>=90%`; pico
  branco. Tema simples: gradiente HSV da cor base, valor `0.72..1.0`, saturação caindo até
  `0.5` no topo.
- Sem áudio: `gRaw` zera depois de ~300 ms sem quadro; status vira `SEM ÁUDIO` após
  ~1500 ms. `SEM CAPTURA` quando o dispositivo/loopback falha.
- `AudioCapture::Poll` mistura todos os canais para mono, aceita float32/int16/int24/int32
  e reabre o dispositivo a cada ~1 s enquanto `ready == false`.

## Windows/GDI gotchas

- WASAPI loopback só entrega pacotes durante playback. Ausência de dados é normal; o app
  trata isso como silêncio. Apps em modo exclusivo/ASIO podem não ser capturados.
- Mutar o **endpoint** não silencia o loopback (o tap vem antes do mute do endpoint). Já o
  volume da sessão/app pode atenuar o sinal capturado; não use volume zero para testar
  silêncio sem confirmar o comportamento da máquina.
- O `ksmedia.h` do MinGW-w64 não define `KSDATAFORMAT_SUBTYPE_IEEE_FLOAT`; a GUID é definida
  manualmente em `src/audio/capture.cpp`. Não adicione `ksmedia.h`/`initguid.h` para
  "consertar" o link.
- `GetBuffer` precisa de buffers válidos e `ReleaseBuffer`; trate `AUDCLNT_S_BUFFER_EMPTY`
  e falhas (dispositivo removido/trocado) com `Shutdown` + retry.
- O timer de 10 ms dispara a ~15.6 ms. Ataque/queda são calculados por tempo decorrido; não
  assuma um tick fixo e não mexa na resolução global do timer do Windows.
- A janela é `WS_POPUP + WS_EX_LAYERED + WS_EX_TOOLWINDOW`. O desenho vai para um DIB 32bpp
  e é entregue por `UpdateLayeredWindow` com alpha pré-multiplicado.
- O alpha do fundo do painel é feito por igualdade exata com `kPanelRgb`
  (`RgbDword(kPanelColor)`, `RGB(6,20,25)`) em `src/gfx/render.cpp`: pixels dessa
  cor recebem a opacidade escolhida (0..100), LEDs/texto/borda ficam opacos e os cantos
  pretos ficam com alpha 1. Lembre que o DIB guarda `0x00RRGGBB` e `COLORREF` guarda
  `0x00BBGGRR`; não compare `pixels[i]` com `kPanelColor` diretamente. Se mudar a cor
  do painel ou usar essa cor em conteúdo, atualize a pós-produção de alpha.
- LEDs e pico precisam ser desenhados com `NULL_PEN`; a caneta padrão do DC deixa um
  contorno preto de 1 px (alpha 1) que vira furo transparente mesmo com opacidade 100%.
  Em opacidade 0 o fundo e os cantos pretos usam alpha 0; só conteúdo fica visível.
- `WM_ERASEBKGND` retorna 1; use `InvalidateRect` e redesenhe o DIB inteiro. Não pinte
  apenas regiões, ou o layered surface fica inconsistente.
- Arrastar é manual (cliente inteiro); não há barra de título. Duplo clique abre opções,
  clique direito abre o menu, bandeja tem o mesmo menu.

## Versioning and git

- `version.txt` é a fonte da verdade (`X.Y.Z`). `src/app/version.h` (`APP_VERSION`) e
  `src/res/app.rc` (`FILEVERSION`, `PRODUCTVERSION`, `FileVersion`, `ProductVersion`)
  devem ser bumpados juntos; o Makefile falha se divergirem.
- Não commite `build/`, `output/`, binários gerados, `GraphicEQ.ini` ou capturas locais.
- O checkout usa `core.filemode=false` e `ignorecase=true` por estar em DrvFs.

## Reference implementations

- `../graph-eq/` — comportamento canônico:
  - `src/com/graph/eq/BandPlan.java`: faixas e rótulos.
  - `src/com/graph/eq/AudioCapture.java`: FFT/escala/tilt.
  - `src/com/graph/eq/EqualizerView.java`: ataque/queda, hold, cores.
  - `Fft.java`, `MainActivity.java`, `EqService.java`: demais referências de UI/serviço.
- `../VU-LED-Windows/` — esqueleto Win32/WASAPI e `AGENTS.md` com gotchas de GDI.
