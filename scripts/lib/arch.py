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
    "ppc32": "PPC",
    "ppc32le": "PPC",
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
    """
    Source: llvm-project/llvm/lib/TargetParser/Triple.cpp
    """

    match arch:
        case "i386" | "i486" | "i586" | "i686":
            return "x86"
        case "amd64" | "x86_64" | "x86_64h":
            return "x86_64"
        case "powerpc" | "powerpcspe" | "ppc" | "ppc32":
            return "ppc"
        case "powerpcle" | "ppcle" | "ppc32le":
            return "ppcle"
        case "powerpc64" | "ppu" | "ppc64":
            return "ppc64"
        case "powerpc64le" | "ppc64le":
            return "ppc64le"
        case "xscale":
            return "arm"
        case "xscaleeb":
            return "armeb"
        case "aarch64" | "arm64" | "arm64e" | "arm64ec":
            return "aarch64"
        case "aarch64_be":
            return "aarch64_be"
        case "aarch64_32" | "arm64_32":
            return "aarch64_32"
        case "arc":
            return "arc"
        case "arm":
            return "arm"
        case "armeb":
            return "armeb"
        case "thumb":
            return "thumb"
        case "thumbeb":
            return "thumbeb"
        case "avr":
            return "avr"
        case "m68k":
            return "m68k"
        case "msp430":
            return "msp430"
        case "mips" | "mipseb" | "mipsallegrex" | "mipsisa32r6" | "mipsr6":
            return "mips"
        case "mipsel" | "mipsallegrexel" | "mipsisa32r6el" | "mipsr6el":
            return "mipsel"
        case "mips64" | "mips64eb" | "mipsn32" | "mipsisa64r6" | "mips64r6" | "mipsn32r6":
            return "mips64"
        case "mips64el" | "mipsn32el" | "mipsisa64r6el" | "mips64r6el" | "mipsn32r6el":
            return "mips64el"
        case "r600":
            return "r600"
        case "amdgcn":
            return "amdgcn"
        case "riscv32":
            return "riscv32"
        case "riscv64":
            return "riscv64"
        case "hexagon":
            return "hexagon"
        case "s390x" | "systemz":
            return "systemz"
        case "sparc":
            return "sparc"
        case "sparcel":
            return "sparcel"
        case "sparcv9" | "sparc64":
            return "sparcv9"
        case "tce":
            return "tce"
        case "tcele":
            return "tcele"
        case "xcore":
            return "xcore"
        case "nvptx":
            return "nvptx"
        case "nvptx64":
            return "nvptx64"
        case "le32":
            return "le32"
        case "le64":
            return "le64"
        case "amdil":
            return "amdil"
        case "amdil64":
            return "amdil64"
        case "hsail":
            return "hsail"
        case "hsail64":
            return "hsail64"
        case "spir":
            return "spir"
        case "spir64":
            return "spir64"
        case "spirv32" | "spirv32v1.0" | "spirv32v1.1" | "spirv32v1.2" | "spirv32v1.3" | "spirv32v1.4" | "spirv32v1.5":
            return "spirv32"
        case "spirv64" | "spirv64v1.0" | "spirv64v1.1" | "spirv64v1.2" | "spirv64v1.3" | "spirv64v1.4" | "spirv64v1.5":
            return "spirv64"
        case "lanai":
            return "lanai"
        case "renderscript32":
            return "renderscript32"
        case "renderscript64":
            return "renderscript64"
        case "shave":
            return "shave"
        case "ve":
            return "ve"
        case "wasm32":
            return "wasm32"
        case "wasm64":
            return "wasm64"
        case "csky":
            return "csky"
        case "loongarch32":
            return "loongarch32"
        case "loongarch64":
            return "loongarch64"
        case "dxil":
            return "dxil"
        case "xtensa":
            return "xtensa"
        case _:
            raise Exception(f"Cannot recognize arch {arch}")
