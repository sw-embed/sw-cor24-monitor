# COR24 Monitor — Build System

tc24r_include := env("HOME") / "github/sw-embed/sw-cor24-x-tinyc/include"
src_c := "src/monitor.c"
src_boot := "src/boot.s"
src_trampoline := "src/trampoline.s"
src_prog_start := "src/prog_start.s"
build_c_s := "build/monitor_c.s"
build_combined := "build/monitor.s"

# Compile C, concatenate boot + C + trampoline, ready to run
build:
    mkdir -p build
    tc24r {{src_c}} -o {{build_c_s}} -I {{tc24r_include}} -I src
    cat {{src_boot}} {{build_c_s}} {{src_trampoline}} > {{build_combined}}

# Build a program: just build-prog programs/echo.c 0x2000
build-prog src addr:
    mkdir -p build
    tc24r {{src}} -o build/prog_c.s -I {{tc24r_include}} -I src
    # Strip tc24r's _start (first 5 lines: .text, .globl _start, _start:, la, jal, _halt, bra)
    sed -n '/^_halt:/,${ /^_halt:/d; /^[[:space:]]*bra[[:space:]]*_halt/d; p; }' build/prog_c.s > build/prog_body.s
    cat {{src_prog_start}} build/prog_body.s > build/prog.s
    cor24-run --assemble build/prog.s build/prog.bin build/prog.lst --base-addr {{addr}}

# Build echo test program at 0x2000
build-echo: build
    just build-prog programs/echo.c 0x2000
    cp build/prog.bin build/echo.bin
    cp build/prog.lst build/echo.lst

# Build ret42 test program at 0x3000
build-ret42: build
    just build-prog programs/ret42.c 0x3000
    cp build/prog.bin build/ret42.bin
    cp build/prog.lst build/ret42.lst

# Build failtest demo program at 0x3000
build-failtest: build
    just build-prog programs/failtest.c 0x3000
    cp build/prog.bin build/failtest.bin
    cp build/prog.lst build/failtest.lst

# Build monitor binary
build-monitor-bin: build
    cor24-run --assemble {{build_combined}} build/monitor.bin build/monitor.lst

# Build exit7 test program at 0x4000
build-exit7: build
    just build-prog programs/exit7.c 0x4000
    cp build/prog.bin build/exit7.bin
    cp build/prog.lst build/exit7.lst

# Build cat demo program at 0x5000
build-cat: build
    just build-prog programs/cat.c 0x5000
    cp build/prog.bin build/cat.bin
    cp build/prog.lst build/cat.lst

# Build everything (test programs)
build-all: build-monitor-bin build-echo build-ret42 build-exit7

# Build demo programs
build-demos: build-monitor-bin build-echo build-failtest build-cat

# Build and run with demo programs loaded
run: build-demos
    cor24-run --load-binary build/monitor.bin@0 --load-binary build/echo.bin@0x2000 --load-binary build/failtest.bin@0x3000 --load-binary build/cat.bin@0x5000 --entry 0 --speed 0 --terminal

# Build and run non-interactive test
test: build-demos
    cor24-run --load-binary build/monitor.bin@0 --load-binary build/echo.bin@0x2000 --load-binary build/failtest.bin@0x3000 --load-binary build/cat.bin@0x5000 --entry 0 --speed 0 --time 1

# Build and run with trace for debugging
test-trace: build-demos
    cor24-run --load-binary build/monitor.bin@0 --load-binary build/echo.bin@0x2000 --load-binary build/failtest.bin@0x3000 --load-binary build/cat.bin@0x5000 --entry 0 --speed 0 --time 1 --trace 100

# Demo: monitor -> sws -> swye editor integration (build + filtered output)
demo-editor:
    bash demos/monitor-editor-demo.sh

# Demo with --dump: full UART log + hex memory dump (shows edited buffer at 0x0F0400)
demo-editor-dump:
    bash demos/monitor-editor-demo.sh --dump

# Clean build artifacts
clean:
    rm -rf build/
