from pathlib import Path
from typing import Iterable, List, Optional, Set

from tap import Tap

from llc_test_parsing import parse_llc_tests


class Args(Tap):
    arch: Optional[str] = None
    triple: Optional[str] = None
    cpu: Optional[str] = None
    attrs: List[str] = []
    global_isel: bool = False

    output: str
    """directory for storing seeds (will create if not exist)"""

    def configure(self):
        self.add_argument("-o", "--output")


def collect_seeds_from_tests(
    out_dir_parent: Path,
    arch_with_sub: Optional[str],
    triple: Optional[str] = None,
    cpu: Optional[str] = None,
    attrs: Set[str] = set(),
    global_isel: bool = False,
    dump_bc: bool = True,
    symlink_to_ll: bool = False,
) -> None:
    if arch_with_sub is None:
        assert triple is not None, f"either arch or triple has to be specified"
        arch_with_sub = triple.split("-")[0]

    llc_tests = parse_llc_tests(
        arch_filter=lambda arch: arch_with_sub.startswith(arch.lower())
    )

    out_dir_parent.mkdir(exist_ok=True)
    out_dir_name = ",".join(get_seeds_dir_name_parts(arch_with_sub, triple, cpu, attrs))
    out_dir = out_dir_parent.joinpath(out_dir_name)
    out_dir.mkdir()

    for test in llc_tests:
        if any(
            cmd.arch_with_sub == arch_with_sub
            and cmd.triple == triple
            and cmd.cpu == cpu
            and cmd.attrs == attrs
            and cmd.global_isel == global_isel
            for cmd in test.runnable_llc_commands
        ):
            if symlink_to_ll:
                out_dir.joinpath(test.path.name).symlink_to(test.path.absolute())

            if dump_bc:
                test.dump_bc(out_dir)


def get_seeds_dir_name_parts(
    arch_with_sub: str,
    triple: Optional[str] = None,
    cpu: Optional[str] = None,
    attrs: Set[str] = set(),
) -> Iterable[str]:
    if triple:
        yield f"triple={triple}"
    else:
        yield f"arch={arch_with_sub}"

    if cpu:
        yield f"cpu={cpu}"

    if len(attrs) > 0:
        yield f"attrs={','.join(attrs)}"


def main() -> None:
    args = Args().parse_args()

    collect_seeds_from_tests(
        out_dir_parent=Path(args.output),
        arch_with_sub=args.arch,
        triple=args.triple,
        cpu=args.cpu,
        attrs=set(args.attrs),
        global_isel=args.global_isel,
    )


if __name__ == "__main__":
    main()
