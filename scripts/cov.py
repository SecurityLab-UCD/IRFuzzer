from pathlib import Path
import subprocess
from typing import Optional

from tap import Tap


class Args(Tap):
    """
    A simple wrapper to run coverage-instrumented executable to ease the process of
    dumping coverage data files (*.gcda) outside object file directories.

    Example: `python scripts/cov.py -o tmp -- ./llvm-project/build-release-cov/bin/llc <llc_args>`
    """

    cmd: list[str]
    """
    Command to run the executable with coverage.
    The executable should be compiled with coverage instrumentation.
    """

    out_dir: Path
    """
    Output directory for coverage data (*.gcda) files.
    """

    def configure(self) -> None:
        self.add_argument("cmd", nargs="+")
        self.add_argument("-o", "--out-dir")


def run_with_coverage(
    cmd: str | list[str],
    cov_out_dir: Path | str,
    stdout: Optional[int] = None,
    dir: Optional[Path] = None,
):
    cwd = dir or Path.cwd()

    subprocess.run(
        cmd,
        env={
            "GCOV_PREFIX_STRIP": str(len(cwd.parts) - 1),
            "GCOV_PREFIX": str(cwd.joinpath(cov_out_dir)),
        },
        check=True,
        stdout=stdout,
        cwd=cwd,
    )


def main():
    args = Args(underscores_to_dashes=True).parse_args()
    run_with_coverage(args.cmd, args.out_dir)


if __name__ == "__main__":
    main()
