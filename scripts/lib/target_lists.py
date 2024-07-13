from lib.target import Target

TARGET_LISTS: dict[str, list[Target]] = {
    "e2e": [
        Target("aarch64"),
        Target("arm"),
        Target("x86_64"),
    ],
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
        Target("systemz"),
    ],
    "3": [
        Target("loongarch32"),
        Target("loongarch64"),
        Target("arc"),
        Target("csky"),
        Target("xcore"),
        Target("sparcv9"),
        Target("ve"),
        Target("bpf"),
    ],
    "4": [
        Target("aarch64_32"),
        Target("aarch64_be"),
        Target("armeb"),
        Target("avr"),
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
        Target("thumb"),
        Target("thumbeb"),
    ],
    "cpu": [
        # AArch64
        ## Apple
        Target("aarch64", "apple-a16"),
        Target("aarch64", "apple-m2"),
        ## ARM
        Target("aarch64", "cortex-a715"),
        Target("aarch64", "cortex-r82"),
        Target("aarch64", "cortex-x3"),
        ## Samsung
        Target("aarch64", "exynos-m5"),
        ## HiSilicon
        Target("aarch64", "tsv110"),
        # AMDGCN
        Target("amdgcn", "gfx1100"),
        Target("amdgcn", "gfx1036"),
        # ARM
        Target("arm", "generic"),
        # Hexagon
        Target("hexagon", "hexagonv71t"),
        Target("hexagon", "hexagonv73"),
        # LoongArch
        Target("loongarch64", "generic-la64"),
        # MIPS
        Target("mips64", "mips64r6"),
        # NVPTX
        Target("nvptx64", "sm_90"),
        # PowerPC
        Target("ppc64", "pwr9"),
        # RISCV
        Target("riscv64", "sifive-u74"),
        Target("riscv64", "sifive-x280"),
        Target("riscv64", "rocket-rv64"),
        # SystemZ
        Target("systemz", "z15"),
        Target("systemz", "z16"),
        # VE
        Target("ve", "generic"),
        ## WASM
        Target("wasm64", "generic"),
        Target("wasm64", "bleeding-edge"),
        # X86
        ## Intel
        Target("x86_64", "alderlake"),  # Alder Lake (12th Gen Core)
        Target("x86_64", "raptorlake"),  # Raptor Lake (13th Gen Core)
        Target("x86_64", "sapphirerapids"),  # Sapphire Rapids (4rd Gen Xeon Scalable)
        Target("x86_64", "emeraldrapids"),  # Emerald Rapids (5rd Gen Xeon)
        ## AMD
        Target("x86_64", "znver3"),
        Target("x86_64", "znver4"),
    ],
}
