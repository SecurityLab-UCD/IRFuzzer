from typing import Iterable, Callable, Iterator, Set, Tuple, TypeVar, Optional, Dict
import os
import subprocess
import logging
from tqdm import tqdm

MATCHER_TABLE_SIZE_DAGISEL = {
    "AArch64": 476832,
    "AMDGPU": 492246,
    "ARC": 1998,
    "ARM": 200669,
    "AVR": 2973,
    "BPF": 3586,
    "CSKY": 19076,
    # DirectX doesn't have SelectionDAG yet?
    "DirectX": None,
    "Hexagon": 177837,
    "Lanai": 2337,
    "LoongArch": 23137,
    "M68k": 18499,
    "MSP430": 9103,
    "Mips": 54044,
    "NVPTX": 185247,
    "PPC": 190302,
    "R600": 37558,
    "RISCV": 2349786,
    "Sparc": 6589,
    "SystemZ": 53271,
    "VE": 71577,
    "WebAssembly": 24807,
    "X86": 685870,
    "XCore": 3854,
}
MATCHER_TABLE_SIZE_GISEL = {
    "AArch64": 199167,
    "AMDGPU": 330592,
    "ARM": 129520,
    "M68k": 2388,
    "Mips": 60449,
    "PPC": 40160,
    "RISCV": 152823,
    "X86": 62440,
}

TRIPLE_ARCH_MAP = {
    "aarch64": "AArch64",
    "aarch64_32": "AArch64",
    "aarch64_be": "AArch64",
    "amdgcn": "AMDGPU",
    "arc": "ARC",
    "arm": "ARM",
    # "arm64": "AArch64",
    # "arm64_32": "AArch64",
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

CPU_ATTR_ARCH_LIST_TIER_1 = [
    ("", "", "aarch64"),
    ("", "", "aarch64_32"),
    ("", "", "amdgcn"),
    # ("", "", "arm64"),
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
    # ("", "", "arm64_32"),
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
    ("cortex-x2", "", "aarch64"),
    ("cortex-r82", "", "aarch64"),
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

LLVM_COMMIT = "01ce71433"


def __verify():
    FUZZING_HOME = os.getenv("FUZZING_HOME")
    if FUZZING_HOME == None:
        logging.error(
            "$FUZZING_HOME not set, why am I running? Did you install correctly?"
        )
        return
    if not os.path.samefile(os.getcwd(), FUZZING_HOME):
        logging.warning("I am not in $FUZZING_HOME now.")
    LLVM = os.getenv("LLVM")
    if LLVM == None:
        logging.warn("$LLVM not set, using llvm-project as default.")
        LLVM = "llvm-project"
    LLVM_path = os.path.join(FUZZING_HOME, LLVM)

    # Verify that current commit in LLVM is LLVM_COMMIT.
    cur_commit = (
        subprocess.check_output(
            ["git", "-C", LLVM_path, "rev-parse", "--short", "HEAD"]
        )
        .decode("ascii")
        .strip()
    )
    if cur_commit != LLVM_COMMIT:
        logging.warn(
            f"Your LLVM version ({cur_commit}) is different from what I have here ({LLVM_COMMIT}), matcher table size maybe incorrect."
        )


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

        try:
            with open(self.get_fuzzer_stats_path(), "r") as f:
                for line in f:
                    line = line.split(" : ")
                    self.__dict__[line[0].strip()] = line[1]
            self.run_time = int(self.run_time)
        except:
            return

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
            for triple_dir in sorted(
                subdirs_of(isel_dir.path), key=lambda dir: dir.name
            ):
                for expr_dir in subdirs_of(triple_dir.path):
                    yield ExprimentInfo(
                        os.path.abspath(expr_dir),
                        fuzzer_dir.name.split(".")[0],
                        isel_dir.name,
                        triple_dir.name,
                        int(expr_dir.name),
                    )
