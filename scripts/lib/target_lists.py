from lib.target import Target

TARGET_LISTS: dict[str, list[Target]] = {
    "1": [
        Target("arm"),
        Target("aarch64"),
        Target("i686"),
        Target("x86_64"),
        Target("riscv32"),
        Target("riscv64"),
        Target("wasm32"),
        Target("wasm64"),
    ],
    "1a": [
        Target("arm", None, "+neon"),
        Target("aarch64", None, "+neon"),
        Target("i686", None, "+avx512f"),
        Target("x86_64", None, "+avx512f"),
        Target("riscv32", None, "+v"),
        Target("riscv64", None, "+v"),
        Target("wasm32", None, "+simd128"),
        Target("wasm64", None, "+simd128"),
    ],
    "2": [
        Target("mips"),
        Target("mips64"),
        Target("ppc"),
        Target("ppc64"),
        Target("amdgcn"),
        Target("nvptx64"),
        Target("hexagon"),
    ],
    "3": [
        Target("aarch64_32"),
        Target("aarch64_be"),
        Target("armeb"),
        Target("avr"),
        Target("bpf"),
        Target("bpfeb"),
        Target("bpfel"),
        Target("lanai"),
        Target("mips64el"),
        Target("mipsel"),
        Target("msp430"),
        Target("nvptx"),
        Target("ppcle"),
        Target("ppc64le"),
        Target("r600"),
        Target("sparc"),
        Target("sparcel"),
        Target("sparcv9"),
        Target("systemz"),
        Target("thumb"),
        Target("thumbeb"),
        Target("ve"),
        Target("xcore"),
    ],
    "cpu": [
        # Intel
        Target("x86_64", "alderlake"),
        Target("x86_64", "sapphirerapids"),
        # AMD
        Target("x86_64", "znver3"),
        # Apple
        Target("aarch64", "apple-a16"),
        Target("aarch64", "apple-m2"),
        # Samsung
        Target("aarch64", "exynos-m5"),
        # ARM
        Target("aarch64", "cortex-a710"),
        Target("aarch64", "cortex-x2"),
        Target("aarch64", "cortex-r82"),
        # Target("aarch64", "neoverse-v2"),
        # AMD
        Target("amdgcn", "gfx1100"),
        Target("amdgcn", "gfx1036"),
        Target("amdgcn", "gfx1010"),
        # Qualcomm
        Target("hexagon", "hexagonv69"),
        # Nvidia
        Target("nvptx64", "sm_90"),
        # SiFive
        Target("riscv64", "sifive-u74"),
        # WASM
        Target("wasm64", "bleeding-edge"),
    ],
}
