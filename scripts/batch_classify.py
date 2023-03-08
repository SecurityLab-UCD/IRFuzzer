import argparse
import logging
import subprocess

from classify import classify
from pathlib import Path

from lib import LLC, LLVM_DIS
from lib.fs import subdirs_of
from lib.llc_command import LLCCommand
from lib.process_concurrency import run_concurrent_subprocesses
from lib.target import Target, TargetFilter

TEMP_FILE = "temp.s"


def classify_wrapper(
    input_dir: Path,
    output_dir: Path,
    target: Target,
    global_isel: bool = False,
    generate_ll_files: bool = True,
) -> None:
    llc_command = LLCCommand(target=target, global_isel=global_isel)
    args = [LLC, *llc_command.get_options(output=TEMP_FILE)]

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

        run_concurrent_subprocesses(
            Path(output_dir).rglob("*.bc"),
            lambda ir_bc_path: subprocess.Popen(
                args=[LLVM_DIS, ir_bc_path],
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
            ),
        )

        print(f"Done generating human-readable IR files for {output_dir}.")


def batch_classify(
    input_root_dir: Path,
    output_root_dir: Path,
    global_isel: bool = False,
    generate_ll_files: bool = True,
    target_filter: TargetFilter = lambda _: True,
) -> None:
    for target_dir in subdirs_of(input_root_dir):
        target = Target.parse(target_dir.name)

        if not target_filter(target):
            continue

        for replicate_dir in subdirs_of(target_dir.path):
            try:
                classify_wrapper(
                    input_dir=Path(replicate_dir.path, "default", "crashes"),
                    output_dir=output_root_dir.joinpath(
                        target_dir.name, replicate_dir.name
                    ),
                    target=target,
                    global_isel=global_isel,
                    generate_ll_files=generate_ll_files,
                )
            except Exception:
                logging.exception(
                    f"Something went wrong when processing {target_dir.path}"
                )


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
                input_root_dir=Path(isel_dir.path),
                output_root_dir=Path(args.output, fuzzer_dir.name, isel_dir.name),
                global_isel=isel_dir.name == "gisel",
            )


if __name__ == "__main__":
    main()
