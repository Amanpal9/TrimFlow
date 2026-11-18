CXX      ?= g++
CXXFLAGS ?= -std=c++17 -Os -Wall -Wextra -ffunction-sections -fdata-sections -D_WIN32_WINNT=0x0A00 -DWINVER=0x0A00
LDFLAGS  ?= -municode -mwindows -static -s -Wl,--gc-sections -Wl,--stack,65536
LIBS     := -lgdi32 -lshell32 -luser32

TrimFlow.exe: src/main.cpp src/heuristics.h
	$(CXX) $(CXXFLAGS) $(LDFLAGS) src/main.cpp -o $@ $(LIBS)

test:
	@mkdir -p build
	$(CXX) -std=c++17 -O2 -Wall -Wextra -Werror tests/heuristics_test.cpp -o build/heuristics_test.exe
	./build/heuristics_test.exe

clean:
	rm -rf build TrimFlow.exe

.PHONY: clean test
