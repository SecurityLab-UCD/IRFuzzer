import logging
import subprocess
from typing import Callable

from classify import classify
from os import path
from pathlib import Path

from common import parallel_subprocess, subdirs_of

LLVM_BIN_PATH = "./llvm-project-fuzzing/build-release/bin"
LLC = path.join(LLVM_BIN_PATH, "llc")
LLVM_DIS = path.join(LLVM_BIN_PATH, "llvm-dis")
TEMP_FILE = "temp.s"


def classify_wrapper(
    mtriple: str,
    input_dir: str,
    output_dir: str,
    global_isel: bool = False,
    generate_ll_files: bool = True,
) -> None:
    args = [LLC, "-mtriple", mtriple, "-o", TEMP_FILE]
    if global_isel:
        args.append("-global-isel")

    print(f"Start classifying {input_dir} using '{(' '.join(args))}'...")

    classify(
        args,
        input_dir,
        output_dir,
        force=True,
        verbose=False,
        create_symlink_to_source=False,
        hash_stacktrace_only=True,
        remove_addr_in_stacktrace=True,
        ignore_undefined_external_symbol=True,
    )

    # remove temp file if exists
    Path(TEMP_FILE).unlink(missing_ok=True)

    print(f"Done classifying {input_dir} using '{(' '.join(args))}'.")

    if generate_ll_files:
        print(f"Generating human-readable IR files for {output_dir}...")

        parallel_subprocess(
            Path(output_dir).rglob("*.bc"),
            64,
            lambda ir_bc_path: subprocess.Popen(
                args=[LLVM_DIS, ir_bc_path],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
            ),
        )

        print(f"Done generating human-readable IR files for {output_dir}.")


def batch_classify(
    input_root_dir: str,
    output_root_dir: str,
    global_isel: bool = False,
    generate_ll_files: bool = True,
    mtriple_filter: Callable[[str], bool] = lambda _: True,
) -> None:
    for subdir in subdirs_of(input_root_dir):
        mtriple = subdir.name

        if not mtriple_filter(mtriple):
            continue

        for subsubdir in subdirs_of(subdir.path):
            try:
                classify_wrapper(
                    mtriple,
                    input_dir=path.join(subsubdir.path, "default", "crashes"),
                    output_dir=path.join(output_root_dir, mtriple, subsubdir.name),
                    global_isel=global_isel,
                    generate_ll_files=generate_ll_files,
                )
            except Exception:
                logging.exception(f"Something went wrong when processing {subdir.path}")


def main() -> None:
    batch_classify(
        input_root_dir="/home/peter/irfuzzer.expriment/baseline/combined/aflisel/dagisel",
        output_root_dir="./crash-classification-7-fuzzing/aflisel/dagisel",
        generate_ll_files=False
    )

    batch_classify(
        input_root_dir="/home/peter/irfuzzer.expriment/baseline/combined/libfuzzer/dagisel",
        output_root_dir="./crash-classification-7-fuzzing/libfuzzer/dagisel",
        generate_ll_files=False
    )


if __name__ == "__main__":
    main()
