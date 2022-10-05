import subprocess
from typing import Optional

from classify import classify
from os import path
from pathlib import Path

LLVM_BIN_PATH = './llvm-project/build-release/bin'
LLC = path.join(LLVM_BIN_PATH, 'llc')
LLVM_DIS = path.join(LLVM_BIN_PATH, 'llvm-dis')
TEMP_FILE = 'temp.s'

def classify_wrapper(
    mtriple: str,
    global_isel: bool = False,
    generate_ll_files: bool = True,
    input_dir: Optional[str] = None,
    output_dir: Optional[str] = None
) -> None:
    args = [LLC, '-mtriple', mtriple, '-o', TEMP_FILE]
    if global_isel:
        args.append('-global-isel')

    if input_dir is None:
        input_dir = path.join(
            'fuzzing',
            '1',
            '-'.join(['fuzzing', 'gisel' if global_isel else 'dagisel', mtriple]),
            'default',
            'crashes'
        )

    if output_dir is None:
        output_dir = path.join(
            'crash-classification',
            '-'.join(['gisel' if global_isel else 'dagisel', mtriple])
        )

    print(f"Start classifying {input_dir} using '{(' '.join(args))}'...")

    classify(args, input_dir, output_dir, force=True)

    # remove temp file if exists
    Path(TEMP_FILE).unlink(missing_ok=True)

    print(f"Done classifying {input_dir} using '{(' '.join(args))}'.")

    if generate_ll_files:
        print(f"Generating human-readable IR files for {output_dir}...")

        for ir_bc_path in Path(output_dir).rglob("*.bc"):
            subprocess.run([LLVM_DIS, ir_bc_path])

        print(f"Done generating human-readable IR files for {output_dir}.")

def batch_classify(*mtriples: str, global_isel: bool = False, generate_ll_files: bool = True) -> None:
    for mtriple in mtriples:
        classify_wrapper(mtriple, global_isel, generate_ll_files)

def main() -> None:
    batch_classify(
        'aarch64',
        'amdgcn',
        'nvptx',
        'riscv32',
        'riscv64',
        'wasm32',
        'wasm64',
        'x86_64'
    )

    batch_classify(
        'aarch64',
        'riscv32',
        'riscv64',
        'x86_64',
        global_isel=True
    )

    # for old folder structure, input dir has to be manually specified
    # classify_wrapper('r600', global_isel=False, input_dir='fuzzing/0/r600/default/crashes')

if __name__ == "__main__":
    main()
