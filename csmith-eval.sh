# Prerequisites: Run ./csmith-gen.sh and ./cross-compile-setup.sh first.

# compile C to LLVM IR bitcode for each optimization level and each architecture
python scripts/batch_compile.py tmpfs/c -o tmpfs/O3-compiled -O 3
python scripts/batch_compile.py tmpfs/c -o tmpfs/O2-compiled -O 2
python scripts/batch_compile.py tmpfs/c -o tmpfs/O1-compiled -O 1
python scripts/batch_compile.py tmpfs/c -o tmpfs/O0-compiled -O 0

# $1: clang target
# $2: llc target
# time is set to 0s so aflplusplus only runs on the seeds,
# which in this case are the architecture-specific LLVM IR bitcode files
# compiled from CSmith generated C code.
fuzz() {
    for O in 0 1 2 3
    do
        python3 scripts/fuzz.py \
            --fuzzer aflplusplus-seeds-dry-run \
            --seeds tmpfs/O${O}-compiled/$1 \
            --targets $2 \
            -t 0s \
            -o tmpfs/csmith-O${O} \
            --on-exist ignore
    done
}

fuzz aarch64 aarch64
fuzz arm arm
fuzz x86_64 x86_64
fuzz i686 i686
fuzz wasm32-wasi wasm32
fuzz riscv64 riscv64
