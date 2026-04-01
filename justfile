# COR24 Monitor — Build System

tc24r_include := env("HOME") / "github/sw-embed/sw-cor24-x-tinyc/include"
src_c := "src/monitor.c"
src_boot := "src/boot.s"
build_c_s := "build/monitor_c.s"
build_combined := "build/monitor.s"

# Compile C, concatenate with boot.s, ready to run
build:
    mkdir -p build
    tc24r {{src_c}} -o {{build_c_s}} -I {{tc24r_include}} -I src
    cat {{src_boot}} {{build_c_s}} > {{build_combined}}

# Build and run interactively
run: build
    cor24-run --run {{build_combined}} --terminal --speed 0

# Build and run, halt after boot banner (non-interactive test)
test: build
    cor24-run --run {{build_combined}} --speed 0 --time 1

# Build binary for loading at address 0
binary: build
    cor24-run --assemble {{build_combined}} build/monitor.bin build/monitor.lst

# Clean build artifacts
clean:
    rm -rf build/
