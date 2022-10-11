from typing import Iterable, Iterator, List, Optional, Set, Tuple
import os
import subprocess
import logging
from tqdm import tqdm

MATCHER_TABLE_SIZE_DAGISEL = {"AVR": 2840, "ARM": 200565, "BPF": 3586, "AArch64": 451325, "AMDGPU":  477347, "R600": 37786, "Hexagon": 174365, "Lanai": 2337, "MSP430": 9103,
                              "Mips": 54044, "NVPTX": 184663, "PPC": 190777, "RISCV": 2079553, "Sparc": 6238, "SystemZ": 53206, "WebAssembly": 61756, "VE": 71557, "X86": 681963, "XCore": 3864}
MATCHER_TABLE_SIZE_GISEL = {"ARM": 129520, "AArch64": 196445, "AMDGPU": 292004,
                            "Mips": 60445, "PPC": 28499, "X86": 62855, "RISCV": 152490}

TRIPLE_ARCH_MAP = {"aarch64": "AArch64",
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
                   "xcore": "XCore"}

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


# Creates subprocesses in parallel.
def parallel_subprocess(iter: Iterable, jobs: int, subprocess_creator, on_exit=None):
    ret = {}
    processes: Set[subprocess.Popen] = set()
    for i in tqdm(iter):
        processes.add(subprocess_creator(i))
        if len(processes) >= jobs:
            # wait for a child process to exit
            os.wait()
            exited_processes = [p for p in processes if p.poll() is not None]
            for p in exited_processes:
                processes.remove(p)
                if on_exit is not None:
                    ret[i] = on_exit(p)
    return ret
