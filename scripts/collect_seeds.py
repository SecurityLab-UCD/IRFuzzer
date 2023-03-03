from pathlib import Path
from typing import Iterable, Literal, Optional

from tap import Tap

from lib.fs import count_files
from lib.llc_test import LLCTest, parse_llc_tests
from lib.target import Target, TargetFilter, TargetProp, create_target_filter
from lib.triple import Triple


class Args(Tap):
    triple: str
    cpu: Optional[str] = None
    attrs: list[str] = []
    global_isel: bool = False
    props_to_match: list[TargetProp] | Literal["all"] = "all"
    seed_format: Literal["bc", "ll"] = "bc"

    output: str
    """directory for storing seeds (will create if not exist)"""

    def configure(self) -> None:
        self.add_argument("-o", "--output")


def get_runnable_llc_tests(
    arch: str,
    global_isel: bool,
    target_filter: TargetFilter = lambda _: True,
) -> Iterable[LLCTest]:
    return (
        test
        for test in parse_llc_tests(arch_filter=lambda a: a == arch)
        if any(
            cmd.global_isel == global_isel and target_filter(cmd.target)
            for cmd in test.runnable_llc_commands
        )
    )


def collect_seeds_from_tests(
    target: Target,
    global_isel: bool,
    out_dir_parent: Path,
    props_to_match: list[TargetProp] | Literal["all"] = "all",
    dump_bc: bool = True,
    symlink_to_ll: bool = False,
) -> Path:
    print(f"Collecting seeds for target {target}...")

    out_dir = out_dir_parent.joinpath(
        "gisel" if global_isel else "dagisel", str(target)
    )
    out_dir.mkdir(parents=True)

    target_filter = (
        (lambda candidate: candidate == target)
        if props_to_match == "all"
        else create_target_filter(target, props_to_match)
    )

    for test in get_runnable_llc_tests(
        arch=target.arch,
        global_isel=global_isel,
        target_filter=target_filter,
    ):
        if symlink_to_ll:
            out_dir.joinpath(test.path.name).symlink_to(test.path.absolute())

        if dump_bc:
            test.dump_bc(out_dir)

    print(f"{count_files(out_dir)} seeds written to {out_dir}.")

    return out_dir


def main() -> None:
    args = Args().parse_args()

    target = Target(
        triple=Triple.parse(args.triple),
        cpu=args.cpu,
        attrs=args.attrs[0] if len(args.attrs) == 1 else args.attrs,
    )

    collect_seeds_from_tests(
        target=target,
        global_isel=args.global_isel,
        out_dir_parent=Path(args.output),
        props_to_match=args.props_to_match,
        dump_bc=args.seed_format == "bc",
        symlink_to_ll=args.seed_format == "ll",
    )


if __name__ == "__main__":
    main()
