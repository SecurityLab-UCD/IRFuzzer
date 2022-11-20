from typing import Iterable, Callable, Iterator, Set, Tuple, TypeVar, Optional, Dict
import os
import subprocess
import logging
from tqdm import tqdm

MATCHER_TABLE_SIZE_DAGISEL = {
    "AArch64": 451325,
    "AMDGPU": 479297,
    "ARM": 200565,
    "AVR": 2840,
    "BPF": 3586,
    "Hexagon": 174265,
    "Lanai": 2337,
    "MSP430": 9103,
    "Mips": 54044,
    "NVPTX": 184663,
    "PPC": 190777,
    "R600": 37786,
    "RISCV": 2079599,
    "Sparc": 6238,
    "SystemZ": 53211,
    "VE": 71557,
    "WebAssembly": 61756,
    "X86": 681963,
    "XCore": 3864,
}
MATCHER_TABLE_SIZE_GISEL = {
    "AArch64": 196445,
    "AMDGPU": 292004,
    "ARM": 129520,
    "Mips": 60445,
    "PPC": 28499,
    "RISCV": 152490,
    "X86": 62855,
}

TRIPLE_ARCH_MAP = {
    "aarch64": "AArch64",
    "aarch64_32": "AArch64",
    "aarch64_be": "AArch64",
    "amdgcn": "AMDGPU",
    "arm": "ARM",
    "arm64": "AArch64",
    "arm64_32": "AArch64",
    "armeb": "ARM",
    "avr": "AVR",
    "bpf": "BPF",
    "bpfeb": "BPF",
    "bpfel": "BPF",
    "hexagon": "Hexagon",
    "lanai": "Lanai",
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

CPU_ATTR_ARCH_LIST_TIER_1 = [
    ("", "", "aarch64"),
    ("", "", "aarch64_32"),
    ("", "", "amdgcn"),
    ("", "", "arm64"),
    ("", "", "bpf"),
    ("", "", "hexagon"),
    ("", "", "mips64"),
    ("", "", "nvptx64"),
    ("", "", "ppc64"),
    ("", "", "riscv64"),
    ("", "", "systemz"),
    ("", "", "thumb"),
    ("", "", "ve"),
    ("", "", "wasm64"),
    ("", "", "i686"),
    ("", "", "x86_64"),
]

CPU_ATTR_ARCH_LIST_TIER_2 = [
    ("", "", "aarch64_be"),
    ("", "", "arm"),
    ("", "", "arm64_32"),
    ("", "", "armeb"),
    ("", "", "avr"),
    ("", "", "bpfeb"),
    ("", "", "bpfel"),
    ("", "", "lanai"),
    ("", "", "mips"),
    ("", "", "mips64el"),
    ("", "", "mipsel"),
    ("", "", "msp430"),
    ("", "", "nvptx"),
    ("", "", "ppc32"),
    ("", "", "ppc32le"),
    ("", "", "ppc64le"),
    ("", "", "r600"),
    ("", "", "riscv32"),
    ("", "", "sparc"),
    ("", "", "sparcel"),
    ("", "", "sparcv9"),
    ("", "", "thumbeb"),
    ("", "", "wasm32"),
    ("", "", "xcore"),
]


CPU_ATTR_ARCH_LIST_TIER_3 = [
    # Intel
    ("alderlake", "", "x86_64"),
    ("sapphirerapids", "", "x86_64"),
    # AMD
    ("znver3", "", "x86_64"),
    # Apple
    ("apple-a16", "", "aarch64"),
    ("apple-m2", "", "aarch64"),
    # Samsung
    ("exynos-m5", "", "aarch64"),
    # ARM
    ("cortex-a710", "", "aarch64"),
    # ("cortex-x2", "", "aarch64"),
    ("cortex-r82", "", "aarch64"),
    ("cortex-m55", "", "aarch64"),
    # ("neoverse-v2", "", "aarch64"),
    # AMD
    ("gfx1100", "", "amdgcn"),
    ("gfx1036", "", "amdgcn"),
    ("gfx1010", "", "amdgcn"),
    # Qualcomm
    ("hexagonv69", "", "hexagon"),
    # Nvidia
    ("sm_90", "", "nvptx64"),
    # SiFive
    ("sifive-u74", "", "riscv64"),
    # WASM
    ("bleeding-edge", "", "wasm64"),
]

LLVM_COMMIT = 66046e6


def __verify():
    FUZZING_HOME = os.getenv("FUZZING_HOME")
    if FUZZING_HOME == None:
        logging.error(
            "$FUZZING_HOME not set, why am I running? Did you install correctly?"
        )
        return
    if os.getcwd() != FUZZING_HOME:
        logging.warning("I am not in $FUZZING_HOME now.")
    LLVM = os.getenv("LLVM")
    if LLVM == None:
        logging.warn("$LLVM not set, using llvm-project as default.")
        LLVM = "llvm-project"
    LLVM = os.path.join(FUZZING_HOME, LLVM)

    # TODO: Verify that current commit in LLVM is COMMIT.


__verify()

__T = TypeVar("__T")
__R = TypeVar("__R")


def parallel_subprocess(
    iter: Iterable[__T],
    jobs: int,
    subprocess_creator: Callable[[__T], subprocess.Popen],
    on_exit: Optional[Callable[[subprocess.Popen], __R]] = None,
) -> Dict[__T, __R]:
    """
    Creates `jobs` subprocesses that run in parallel.
    `iter` contains input that is send to each subprocess.
    `subprocess_creator` creates the subprocess and returns a `Popen`.
    After each subprocess ends, `on_exit` will go collect user defined input and return.
    The return valus is a dictionary of inputs and outputs.

    User has to guarantee elements in `iter` is unique, or the output may be incorrect.
    """
    ret = {}
    processes: Set[Tuple[subprocess.Popen, __T]] = set()
    for input in tqdm(iter):
        processes.add((subprocess_creator(input), input))
        if len(processes) >= jobs:
            # wait for a child process to exit
            os.wait()
            exited_processes = [(p, i) for p, i in processes if p.poll() is not None]
            for p, i in exited_processes:
                processes.remove((p, i))
                if on_exit is not None:
                    ret[i] = on_exit(p)
    # wait for remaining processes to exit
    for p, i in processes:
        p.wait()
        if on_exit is not None:
            ret[i] = on_exit(p)
    return ret


def subdirs_of(dir: str) -> Iterator[os.DirEntry]:
    return (f for f in os.scandir(dir) if f.is_dir())


IRFUZZER_DATA_ENV = "IRFUZZER_DATA"


class ExprimentInfo:
    expr_path: str
    fuzzer: str
    isel: str
    arch: str
    expr_id: int

    def __init__(self, expr_path, fuzzer, isel, arch, expr_id):
        self.expr_path = expr_path
        self.fuzzer = fuzzer
        self.isel = isel
        self.arch = arch
        self.expr_id = expr_id

    def to_expr_path(self):
        return self.expr_path

    def to_arch_path(self):
        return os.path.join(self.expr_path, "..")

    def get_plot_data_path(self):
        return os.path.join(self.to_expr_path(), "default", "plot_data")

    def get_fuzzer_stats_path(self):
        return os.path.join(self.to_expr_path(), "default", "fuzzer_stats")


def for_all_expriments(archive_path: str):
    for fuzzer_dir in subdirs_of(archive_path):
        for isel_dir in subdirs_of(fuzzer_dir.path):
            for arch_dir in subdirs_of(isel_dir.path):
                for expr_dir in subdirs_of(arch_dir.path):
                    yield ExprimentInfo(
                        os.path.abspath(expr_dir),
                        fuzzer_dir.name.split(".")[0],
                        isel_dir.name,
                        arch_dir.name,
                        int(expr_dir.name),
                    )
