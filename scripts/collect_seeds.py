import os
from pathlib import Path
from typing import Iterable, List, Optional, Set

from tap import Tap

from llc_test_parsing import Triple, parse_llc_tests
from common import TRIPLE_ARCH_MAP


class Args(Tap):
    triple: str
    cpu: Optional[str] = None
    attrs: List[str] = []
    global_isel: bool = False

    output: str
    """directory for storing seeds (will create if not exist)"""

    def configure(self):
        self.add_argument("-o", "--output")


def get_seeds_dir_name(
    triple: Triple,
    cpu: Optional[str] = None,
    attrs: Set[str] = set(),
    global_isel: bool = False,
) -> str:
    def get_parts() -> Iterable[str]:
        yield str(triple)

        if cpu:
            yield f"cpu={cpu}"

        for attr in attrs:
            yield attr

        if global_isel:
            yield "gisel"

    return ",".join(get_parts())


def collect_seeds_from_tests(
    out_dir_parent: Path,
    triple: Triple,
    cpu: Optional[str] = None,
    attrs: Set[str] = set(),
    global_isel: bool = False,
    dump_bc: bool = True,
    symlink_to_ll: bool = False,
) -> Path:
    llc_tests = parse_llc_tests(
        arch_filter=lambda arch: arch == TRIPLE_ARCH_MAP[triple.arch_with_sub]
    )

    out_dir_parent.mkdir(exist_ok=True)
    out_dir_name = get_seeds_dir_name(triple, cpu, attrs, global_isel)
    out_dir = out_dir_parent.joinpath(out_dir_name)
    out_dir.mkdir()

    for test in llc_tests:
        if any(
            cmd.triple == triple
            and cmd.cpu == cpu
            and cmd.attrs == attrs
            and cmd.global_isel == global_isel
            for cmd in test.runnable_llc_commands
        ):
            if symlink_to_ll:
                out_dir.joinpath(test.path.name).symlink_to(test.path.absolute())

            if dump_bc:
                test.dump_bc(out_dir)

    return out_dir


def count_files_in_dir(dir: Path) -> int:
    return len(next(os.walk(dir))[2])


def main() -> None:
    args = Args().parse_args()

    out_dir = collect_seeds_from_tests(
        out_dir_parent=Path(args.output),
        triple=Triple.parse(args.triple),
        cpu=args.cpu,
        attrs=set(args.attrs),
        global_isel=args.global_isel,
    )

    print(f"{count_files_in_dir(out_dir)} seeds written to {out_dir}.")


if __name__ == "__main__":
    main()
