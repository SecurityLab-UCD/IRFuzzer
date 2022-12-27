import argparse
import logging
import subprocess
from typing import Callable, Optional

from classify import classify
from os import path
from pathlib import Path

from common import parallel_subprocess, subdirs_of

LLVM_BIN_PATH = "./llvm-project-fuzzing/build-release/bin"
LLC = path.join(LLVM_BIN_PATH, "llc")
LLVM_DIS = path.join(LLVM_BIN_PATH, "llvm-dis")
TEMP_FILE = "temp.s"


def classify_wrapper(
    input_dir: str,
    output_dir: str,
    mtriple: str,
    mcpu: Optional[str] = None,
    global_isel: bool = False,
    generate_ll_files: bool = True,
) -> None:
    args = [LLC, f"-mtriple={mtriple}"]

    if mcpu is not None:
        args.append(f"-mcpu={mcpu}")

    if global_isel:
        args.append("-global-isel")

    args += ["-o", TEMP_FILE]

    print(f"Start classifying {input_dir} using '{(' '.join(args))}'...")

    classify(
        args,
        input_dir,
        output_dir,
        force=True,
        verbose=False,
        create_symlink_to_source=True,
        hash_stacktrace_only=True,
        hash_op_code_only_for_isel_crash=True,
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
        parts = subdir.name.split("-", 1)

        mtriple = parts[0]
        mcpu = parts[1] if len(parts) == 2 else None

        if not mtriple_filter(mtriple):
            continue

        for subsubdir in subdirs_of(subdir.path):
            try:
                classify_wrapper(
                    input_dir=path.join(subsubdir.path, "default", "crashes"),
                    output_dir=path.join(output_root_dir, subdir.name, subsubdir.name),
                    mtriple=mtriple,
                    mcpu=mcpu,
                    global_isel=global_isel,
                    generate_ll_files=generate_ll_files,
                )
            except Exception:
                logging.exception(f"Something went wrong when processing {subdir.path}")


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Batch classify LLVM crashes",
    )

    parser.add_argument(
        "-i",
        "--input",
        type=str,
        required=True,
        help="The input directory containing all fuzzer directories",
    )

    parser.add_argument(
        "-o",
        "--output",
        type=str,
        required=True,
        help="The output directory",
    )

    args = parser.parse_args()

    for fuzzer_dir in subdirs_of(args.input):
        for isel_dir in subdirs_of(fuzzer_dir.path):
            batch_classify(
                input_root_dir=path.join(isel_dir.path),
                output_root_dir=path.join(args.output, fuzzer_dir.name, isel_dir.name),
                global_isel=isel_dir.name == "gisel",
            )


if __name__ == "__main__":
    main()
