import subprocess
from typing import Optional

from classify import classify
from os import path
from pathlib import Path

from common import parallel_subprocess, subdirs_of

LLVM_BIN_PATH = "./llvm-project/build-release/bin"
LLC = path.join(LLVM_BIN_PATH, "llc")
LLVM_DIS = path.join(LLVM_BIN_PATH, "llvm-dis")
TEMP_FILE = "temp.s"


def classify_wrapper(
    mtriple: str,
    global_isel: bool = False,
    generate_ll_files: bool = True,
    input_dir: Optional[str] = None,
    output_dir: Optional[str] = None,
) -> None:
    args = [LLC, "-mtriple", mtriple, "-o", TEMP_FILE]
    if global_isel:
        args.append("-global-isel")

    if input_dir is None:
        input_dir = path.join(
            "fuzzing",
            "1",
            "-".join(["fuzzing", "gisel" if global_isel else "dagisel", mtriple]),
            "default",
            "crashes",
        )

    if output_dir is None:
        output_dir = path.join(
            "crash-classification",
            "-".join(["gisel" if global_isel else "dagisel", mtriple]),
        )

    print(f"Start classifying {input_dir} using '{(' '.join(args))}'...")

    classify(args, input_dir, output_dir, force=True)

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
    *mtriples: str, global_isel: bool = False, generate_ll_files: bool = True
) -> None:
    for mtriple in mtriples:
        classify_wrapper(mtriple, global_isel, generate_ll_files)


def batch_classify_v3(
    input_root_dir: str,
    output_root_dir: str,
    global_isel: bool = False,
    generate_ll_files: bool = True,
) -> None:
    for subdir in subdirs_of(input_root_dir):
        mtriple = subdir.name
        if mtriple == "r600":
            continue
        for subsubdir in subdirs_of(subdir.path):
            classify_wrapper(
                mtriple,
                global_isel,
                generate_ll_files,
                input_dir=path.join(subsubdir.path, "default", "crashes"),
                output_dir=path.join(output_root_dir, mtriple),
            )


def main() -> None:
    batch_classify_v3(
        input_root_dir="./fuzzing-2/2-aflisel-dagisel/aflisel/dagisel",
        output_root_dir="./crash-classification-2/dagisel",
    )

    batch_classify_v3(
        input_root_dir="./fuzzing-2/2-aflisel-gisel/aflisel/gisel",
        output_root_dir="./crash-classification-2/gisel",
        global_isel=True,
    )

    # batch_classify(
    #     'aarch64',
    #     'amdgcn',
    #     'nvptx',
    #     'riscv32',
    #     'riscv64',
    #     'wasm32',
    #     'wasm64',
    #     'x86_64'
    # )

    # batch_classify(
    #     'aarch64',
    #     'riscv32',
    #     'riscv64',
    #     'x86_64',
    #     global_isel=True
    # )


if __name__ == "__main__":
    main()
