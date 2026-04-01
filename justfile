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

# Build monitor binary
build-monitor-bin: build
    cor24-run --assemble {{build_combined}} build/monitor.bin build/monitor.lst

# Build exit7 test program at 0x4000
build-exit7: build
    just build-prog programs/exit7.c 0x4000
    cp build/prog.bin build/exit7.bin
    cp build/prog.lst build/exit7.lst

# Build everything
build-all: build-monitor-bin build-echo build-ret42 build-exit7

# Build and run with programs loaded
run: build-all
    cor24-run --load-binary build/monitor.bin@0 --load-binary build/echo.bin@0x2000 --load-binary build/ret42.bin@0x3000 --load-binary build/exit7.bin@0x4000 --entry 0 --speed 0 --terminal

# Build and run non-interactive test
test: build-all
    cor24-run --load-binary build/monitor.bin@0 --load-binary build/echo.bin@0x2000 --load-binary build/ret42.bin@0x3000 --load-binary build/exit7.bin@0x4000 --entry 0 --speed 0 --time 1

# Build and run with trace for debugging
test-trace: build-all
    cor24-run --load-binary build/monitor.bin@0 --load-binary build/echo.bin@0x2000 --load-binary build/ret42.bin@0x3000 --load-binary build/exit7.bin@0x4000 --entry 0 --speed 0 --time 1 --trace 100

# Clean build artifacts
clean:
    rm -rf build/
