ARCH_TO_BACKEND_MAP: dict[str, str] = {
    "aarch64": "AArch64",
    "aarch64_32": "AArch64",
    "aarch64_be": "AArch64",
    "amdgcn": "AMDGPU",
    "arc": "ARC",
    "arm": "ARM",
    "armeb": "ARM",
    "avr": "AVR",
    "bpf": "BPF",
    "bpfeb": "BPF",
    "bpfel": "BPF",
    "csky": "CSKY",
    "hexagon": "Hexagon",
    "lanai": "Lanai",
    "loongarch32": "LoongArch",
    "loongarch64": "LoongArch",
    "m68k": "M68k",
    "mips": "Mips",
    "mips64": "Mips",
    "mips64el": "Mips",
    "mipsel": "Mips",
    "msp430": "MSP430",
    "nvptx": "NVPTX",
    "nvptx64": "NVPTX",
    "ppc": "PPC",
    "ppcle": "PPC",
    "ppc64": "PPC",
    "ppc64le": "PPC",
    "r600": "R600",
    "riscv32": "RISCV",
    "riscv64": "RISCV",
    "sparc": "Sparc",
    "sparcel": "Sparc",
    "sparcv9": "Sparc",
    "systemz": "SystemZ",
    "thumb": "ARM",
    "thumbeb": "ARM",
    "ve": "VE",
    "wasm32": "WebAssembly",
    "wasm64": "WebAssembly",
    "i686": "X86",
    "x86_64": "X86",
    "xcore": "XCore",
}


def normalize_arch(arch: str) -> str:
    match arch:
        case "aarch64" | "arm64":
            return "aarch64"
        case "aarch64_32" | "arm64_32":
            return "aarch64_32"
        case "powerpc" | "ppc" | "ppc32":
            return "ppc"
        case "powerpcle" | "ppcle" | "ppc32le":
            return "ppcle"
        case "powerpc64" | "ppc64":
            return "ppc64"
        case "powerpc64le" | "ppc64le":
            return "ppc64le"
        case "s390x" | "systemz":
            return "systemz"
        case _:
            return arch
