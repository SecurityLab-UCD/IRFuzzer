from io import TextIOWrapper
import multiprocessing
import re
import subprocess
from pathlib import Path
from typing import Optional

from tap import Tap
from lib import LLVM
from lib.fs import subdirs_of


LLVM_AFL_BUILD_PATH = Path(LLVM, "build-afl")


class Args(Tap):
    jobs: int = multiprocessing.cpu_count()
    output: Optional[str] = "scripts/lib/matcher_table_sizes.py"

    def configure(self) -> None:
        self.add_argument("-o", "--output")


def get_obj_file_suffix(global_isel: bool) -> str:
    return "InstructionSelector" if global_isel else "ISelDAGToDAG"


def remove_matcher_table_build_files(global_isel: bool) -> None:
    suffix = get_obj_file_suffix(global_isel)

    for target_dir in subdirs_of(Path(LLVM_AFL_BUILD_PATH, "lib/Target")):
        if not target_dir.is_dir() or target_dir.name == "CMakeFiles":
            continue

        backend = target_dir.name
        paths = list(Path(target_dir).glob(
            f"CMakeFiles/LLVM{backend}CodeGen.dir/**/*{suffix}.cpp.o"
        ))

        for path in paths:
            print(f"Removing {path}...")
            path.unlink()


def build_llvm_afl(jobs: int) -> str:
    args = ["ninja", "-j", str(jobs)]
    print(" ".join(args))

    p = subprocess.run(
        args=args,
        cwd=LLVM_AFL_BUILD_PATH,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )

    stdout = p.stdout.decode("utf-8")
    stderr = p.stderr.decode("utf-8")

    if len(stderr) > 0:
        print(stderr)
        exit(1)

    print(stdout)

    p.check_returncode()

    return stdout


def get_output_pattern(global_isel: bool) -> str:
    suffix = get_obj_file_suffix(global_isel)
    line1 = rf"\[(\d+)/\d+\] Building CXX object lib/Target/(.+)/CMakeFiles/.+{suffix}\.cpp\.o"
    line2 = r"\[\+\] MatcherTable size: (\d+)"
    return rf"{line1}\n{line2}"


def extract_matcher_table_size(stdout: str, global_isel: bool) -> dict[str, int]:
    matches = re.findall(get_output_pattern(global_isel), stdout)

    table_sizes = {}

    for match in matches:
        backend = match[1]
        table_size = int(match[2])
        table_sizes[backend] = table_size

    return table_sizes


def dump_py(
    name: str, dict: dict[str, int], file: Optional[TextIOWrapper] = None
) -> None:
    print(name + ": dict[str, int] = {", file=file)
    for key in sorted(dict.keys()):
        print(f'    "{key}": {dict[key]},', file=file)
    print("}", file=file)


def main() -> None:
    args = Args().parse_args()

    remove_matcher_table_build_files(global_isel=False)
    remove_matcher_table_build_files(global_isel=True)

    stdout = build_llvm_afl(jobs=args.jobs)

    dag_isel_table_sizes = extract_matcher_table_size(stdout, global_isel=False)
    global_isel_table_sizes = extract_matcher_table_size(stdout, global_isel=True)

    f = open(args.output, "w") if args.output and args.output != "-" else None

    dump_py("DAGISEL_MATCHER_TABLE_SIZES", dag_isel_table_sizes, file=f)
    print(file=f)
    dump_py("GISEL_MATCHER_TABLE_SIZES", global_isel_table_sizes, file=f)

    if f:
        f.close()


if __name__ == "__main__":
    main()
