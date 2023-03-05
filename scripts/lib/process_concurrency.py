import multiprocessing
import os
import subprocess
from typing import Callable, Iterable, Optional, Tuple, TypeVar

from tqdm import tqdm


MAX_SUBPROCESSES = max(multiprocessing.cpu_count() - 2, 1)

__T = TypeVar("__T")
__R = TypeVar("__R")


def run_concurrent_subprocesses(
    iter: Iterable[__T],
    subprocess_creator: Callable[[__T], subprocess.Popen],
    on_exit: Optional[Callable[[subprocess.Popen], __R]] = None,
    max_jobs: int = MAX_SUBPROCESSES,
) -> dict[__T, __R]:
    """
    Creates up to `max_jobs` subprocesses that run concurrently.
    `iter` contains inputs that is send to each subprocess.
    `subprocess_creator` creates the subprocess and returns a `Popen`.
    After each subprocess ends, `on_exit` will go collect user defined input and return.
    The return valus is a dictionary of inputs and outputs.

    User has to guarantee elements in `iter` is unique, or the output may be incorrect.
    """
    ret = {}
    processes: set[Tuple[subprocess.Popen, __T]] = set()
    for input in tqdm(iter):
        processes.add((subprocess_creator(input), input))
        if len(processes) >= max_jobs:
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
