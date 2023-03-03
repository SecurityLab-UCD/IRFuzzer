from lib.target import Target


TARGET_LIST_TIER_1: list[Target] = [
    Target("aarch64"),
    Target("amdgcn"),
    Target("bpf"),
    Target("hexagon"),
    Target("mips64"),
    Target("nvptx64"),
    Target("ppc64"),
    Target("riscv32"),
    Target("riscv64"),
    Target("systemz"),
    Target("thumb"),
    Target("ve"),
    Target("wasm32"),
    Target("wasm64"),
    Target("i686"),
    Target("x86_64"),
]

TARGET_LIST_TIER_2: list[Target] = [
    Target("aarch64_32"),
    Target("aarch64_be"),
    # Target("arm64"),
    Target("arm"),
    # Target("arm64_32"),
    Target("armeb"),
    Target("avr"),
    Target("bpfeb"),
    Target("bpfel"),
    Target("lanai"),
    Target("mips"),
    Target("mips64el"),
    Target("mipsel"),
    Target("msp430"),
    Target("nvptx"),
    Target("ppc32"),
    Target("ppc32le"),
    Target("ppc64le"),
    Target("r600"),
    Target("riscv32"),
    Target("sparc"),
    Target("sparcel"),
    Target("sparcv9"),
    Target("thumbeb"),
    Target("wasm32"),
    Target("xcore"),
]


TARGET_LIST_TIER_3: list[Target] = [
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
]
