import logging
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
    on_exit: Optional[Callable[[__T, Optional[int], subprocess.Popen], __R]] = None,
    max_jobs: int = MAX_SUBPROCESSES,
) -> dict[__T, __R]:
    """
    Creates up to `max_jobs` subprocesses that run concurrently.
    `iter` contains inputs that is used to start each subprocess.
    `subprocess_creator` creates the subprocess and returns a `Popen`.
    After each subprocess ends, `on_exit` will go collect user defined input and return.
    The return valus is a dictionary of inputs and outputs.

    User has to guarantee elements in `iter` is unique, or the output may be incorrect.
    """
    ret: dict[__T, __R] = {}
    processes: dict[int, Tuple[subprocess.Popen, __T]] = dict()

    def wait_next() -> None:
        pid, status = os.wait()
        p, i = processes.pop(pid)

        exit_code: Optional[int] = None

        if os.WIFEXITED(status):
            exit_code = os.WEXITSTATUS(status)
            logging.debug(f"Child process {pid} exited with code {exit_code}.")
        else:
            logging.debug(f"Child process {pid} exited abnormally.")

        if on_exit is not None:
            ret[i] = on_exit(i, exit_code, p)

    for input in tqdm(iter):
        p = subprocess_creator(input)
        processes[p.pid] = (p, input)

        if len(processes) >= max_jobs:
            wait_next()

    # wait for remaining processes to exit
    while len(processes) > 0:
        wait_next()

    return ret
