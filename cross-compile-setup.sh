# Acquire headers for cross-compiling to various architectures

sudo apt install libc6-dev-i386-cross libc6-dev-amd64-cross libc6-dev-armel-cross libc6-dev-arm64-cross libc6-dev-riscv64-cross

# https://github.com/riscv-software-src/riscv-pk/issues/125
(
    cd /usr/riscv64-linux-gnu/include/gnu
    sudo cp stubs-lp64d.h stubs-lp64.h
)

# https://github.com/WebAssembly/wasi-sdk
(
    cd ~
    wget https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-20/wasi-sysroot-20.0.tar.gz
    tar xvf wasi-sysroot-20.0.tar.gz
)
