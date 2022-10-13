from typing import Iterable, Callable, Set, TypeVar, Optional, Dict
import os
import subprocess
import logging
from tqdm import tqdm

MATCHER_TABLE_SIZE_DAGISEL = {
    "AArch64": 451325,
    "AMDGPU":  479297,
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
    "arm64": "ARM",
    "arm64_32": "ARM",
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
    "x86": "X86",
    "x86-64": "X86",
    "xcore": "XCore",
}

TRIPLE_ARCH_MAP_TIER_1 = {
    "aarch64": "AArch64",
    "aarch64_32": "AArch64",
    "amdgcn": "AMDGPU",
    "arm64": "ARM",
    "bpf": "BPF",
    "hexagon": "Hexagon",
    "mips64": "Mips",
    "nvptx64": "NVPTX",
    "ppc64": "PPC",
    "riscv64": "RISCV",
    "systemz": "SystemZ",
    "thumb": "ARM",
    "ve": "VE",
    "wasm64": "WebAssembly",
    "x86": "X86",
    "x86-64": "X86",
}

TRIPLE_ARCH_MAP_TIER_2 = {
    "aarch64_be": "AArch64",
    "arm": "ARM",
    "arm64_32": "ARM",
    "armeb": "ARM",
    "avr": "AVR",
    "bpfeb": "BPF",
    "bpfel": "BPF",
    "lanai": "Lanai",
    "mips": "Mips",
    "mips64el": "Mips",
    "mipsel": "Mips",
    "msp430": "MSP430",
    "nvptx": "NVPTX",
    "ppc32": "PPC",
    "ppc32le": "PPC",
    "ppc64le": "PPC",
    "r600": "R600",
    "riscv32": "RISCV",
    "sparc": "Sparc",
    "sparcel": "Sparc",
    "sparcv9": "Sparc",
    "thumbeb": "ARM",
    "wasm32": "WebAssembly",
    "xcore": "XCore"
}

LLVM_COMMIT = 66046e6


def __verify():
    FUZZING_HOME = os.getenv("FUZZING_HOME")
    if FUZZING_HOME == None:
        logging.error(
            "$FUZZING_HOME not set, why am I running? Did you install correctly?")
    if os.getcwd() != FUZZING_HOME:
        logging.warning("I am not in $FUZZING_HOME now.")
    LLVM = os.getenv("LLVM")
    if LLVM == None:
        logging.warn("$LLVM not set, using llvm-project as default.")
    LLVM = os.path.join(FUZZING_HOME, LLVM)

    # TODO: Verify that current commit in LLVM is COMMIT.


__verify()

__T = TypeVar('__T')
__R = TypeVar('__R')


def parallel_subprocess(
        iter: Iterable[__T],
        jobs: int,
        subprocess_creator: Callable[[__T], subprocess.Popen],
        on_exit: Optional[Callable[[subprocess.Popen], __R]] = None
) -> Dict[__T, __R]:
    '''
    Creates `jobs` subprocesses that run in parallel.
    `iter` contains input that is send to each subprocess.
    `subprocess_creator` creates the subprocess and returns a `Popen`.
    After each subprocess ends, `on_exit` will go collect user defined input and return.
    The return valus is a dictionary of inputs and outputs.

    User has to guarantee elements in `iter` is unique, or the output may be incorrect.
    '''
    ret = {}
    processes: Set[(subprocess.Popen, __T)] = set()
    for input in tqdm(iter):
        processes.add((subprocess_creator(input), input))
        if len(processes) >= jobs:
            # wait for a child process to exit
            os.wait()
            exited_processes = [
                (p, i) for p, i in processes if p.poll() is not None]
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
