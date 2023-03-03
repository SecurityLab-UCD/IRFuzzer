
import os
from pathlib import Path
from typing import Iterator


def subdirs_of(dir: Path | str) -> Iterator[os.DirEntry]:
    return (f for f in os.scandir(dir) if f.is_dir())


def count_files(dir: Path) -> int:
    """
    count number of file in the specified directory (not including sub-directories)
    """
    return len(next(os.walk(dir))[2])
