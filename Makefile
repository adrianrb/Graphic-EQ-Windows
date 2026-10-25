# Graphic EQ — cross-compile MinGW-w64 a partir do WSL.
#
#   make          compila src/**/*.cpp + src/res/app.rc -> output/GraphicEQ.exe
#   make test     compila e roda o teste de DSP no host (g++)
#   make clean    apaga build/ e output/
#
# -static é OBRIGATÓRIO: sem ele o .exe depende de libstdc++-6.dll,
# libgcc_s_seh-1.dll e libwinpthread-1.dll, que não existem no Windows.

CXX      := x86_64-w64-mingw32-g++
CXXFLAGS := -O2 -std=c++17 -Wall -municode -mwindows -I src
LDFLAGS  := -static -municode -mwindows
LDLIBS   := -lole32 -luuid -lgdi32 -luser32 -lcomdlg32 -lcomctl32 \
            -ladvapi32 -lshell32
RC       := x86_64-w64-mingw32-windres
RCFLAGS  := -I src/res

SRCS := $(wildcard src/*.cpp src/*/*.cpp)
OBJS := $(SRCS:src/%.cpp=build/%.o)
DEPS := $(OBJS:.o=.d)
RES  := build/app.res
OUT  := output/GraphicEQ.exe
VER  := $(shell cat version.txt)
comma := ,

.PHONY: all clean test check-version

all: check-version $(OUT)

# version.txt é a fonte da verdade; src/app/version.h é o que o app compila.
check-version:
	@grep -q 'APP_VERSION = L"$(VER)"' src/app/version.h || \
	  { echo "erro: version.txt ($(VER)) diverge de src/app/version.h"; exit 1; }
	@grep -q 'FILEVERSION $(subst .,$(comma),$(VER)),0' src/res/app.rc || \
	  { echo "erro: version.txt ($(VER)) diverge do FILEVERSION em src/res/app.rc"; exit 1; }
	@grep -q 'ProductVersion", "$(VER)"' src/res/app.rc || \
	  { echo "erro: version.txt ($(VER)) diverge do ProductVersion em src/res/app.rc"; exit 1; }

build/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

$(RES): src/res/app.rc src/res/app.ico src/res/resource.h | build
	$(RC) $(RCFLAGS) -O coff -o $@ src/res/app.rc

$(OUT): $(OBJS) $(RES) | output
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

build output:
	mkdir -p $@

test: check-version
	@mkdir -p build
	g++ -std=c++17 -O2 -Wall -Wextra -I src \
	  tests/dsp_test.cpp src/dsp/fft.cpp src/dsp/band_plan.cpp \
	  src/dsp/spectrum.cpp -o build/dsp_test
	./build/dsp_test

-include $(DEPS)

clean:
	rm -rf build output
