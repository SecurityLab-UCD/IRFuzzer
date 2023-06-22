import logging
import os
from pathlib import Path
import subprocess


FUZZING_HOME = os.getenv(key="FUZZING_HOME")
LLVM = os.getenv(key="LLVM", default="llvm-project")

LLVM_BIN_PATH = Path(LLVM, "build-release/bin")
LLVM_LOOKUP_TABLE_DIR = Path(LLVM, "build-pl", "pattern-lookup")
LLC = Path(LLVM_BIN_PATH, "llc")
LLVM_AS = Path(LLVM_BIN_PATH, "llvm-as")
LLVM_DIS = Path(LLVM_BIN_PATH, "llvm-dis")

IRFUZZER_DATA_ENV = "IRFUZZER_DATA"


def __verify_working_dir():
    if FUZZING_HOME is None:
        logging.error(
            "$FUZZING_HOME not set, why am I running? Did you install correctly?"
        )
        exit(1)

    if not os.path.samefile(os.getcwd(), FUZZING_HOME):
        logging.warning("I am not in $FUZZING_HOME now.")


def __verify_llvm_version():
    expected_commit = "bcb8a9450388"

    actual_commit = (
        subprocess.check_output(["git", "-C", LLVM, "rev-parse", "--short", "HEAD"])
        .decode("ascii")
        .strip()
    )

    if actual_commit != expected_commit:
        logging.warn(
            f"Your LLVM version {actual_commit} is not {expected_commit}."
            " Matcher table sizes may be incorrect."
        )


__verify_working_dir()
__verify_llvm_version()
