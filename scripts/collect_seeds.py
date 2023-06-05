import logging
from pathlib import Path
import subprocess
from typing import Iterable, Literal, Optional

from tap import Tap

from lib.fs import count_files
from lib.llc_command import LLCCommand
from lib.llc_test import LLCTest, parse_llc_tests
from lib.target import Target, TargetFilter, TargetProp, create_target_filter
from lib.triple import Triple


class Args(Tap):
    triple: str
    cpu: Optional[str] = None
    attrs: list[str] = []
    global_isel: bool = False

    props_to_match: list[TargetProp] = ["triple", "cpu", "attrs"]
    """
    the properties of a test target to match those of the fuzzing target,
    used to determine which tests should be included as seeds.
    """

    seed_format: Literal["bc", "ll"] = "bc"
    """
    whether to create symlinks to the tests, or assemble to bitcode (*.bc) files.
    """

    timeout: Optional[float] = None
    """
    only include test cases that can be compiled within the specified in seconds.
    """

    output: str
    """directory for storing seeds (will create if not exist)"""

    def configure(self) -> None:
        self.add_argument("-o", "--output")


def get_runnable_llc_tests(
    backend: str,
    global_isel: bool,
    target_filter: TargetFilter = lambda _: True,
) -> Iterable[LLCTest]:
    return (
        test
        for test in parse_llc_tests(backend_filter=lambda a: a == backend)
        if any(
            cmd.global_isel == global_isel and target_filter(cmd.target)
            for cmd in test.runnable_llc_commands
        )
    )


def validate_seed(
    seed_path: Path, llc_command: LLCCommand, timeout_secs: Optional[float] = None
) -> bool:
    try:
        subprocess.run(
            llc_command.get_args(input=seed_path, output="-"),
            timeout=timeout_secs,
            check=True,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )

        return True
    except subprocess.CalledProcessError:
        logging.warning(f"Seed candidate {seed_path} does not compile.")
    except subprocess.TimeoutExpired:
        logging.warning(f"Seed candidate {seed_path} timed out when compiling.")

    return False


def collect_seeds_from_tests(
    target: Target,
    global_isel: bool,
    out_dir_parent: Path,
    props_to_match: list[TargetProp] = ["triple", "cpu", "attrs"],
    dump_bc: bool = True,
    symlink_to_ll: bool = False,
    timeout_secs: Optional[float] = None,
) -> Path:
    print(f"Collecting seeds for target {target}...")

    out_dir = out_dir_parent.joinpath(
        "gisel" if global_isel else "dagisel", str(target)
    )

    try:
        out_dir.mkdir(parents=True)
    except FileExistsError:
        logging.warning(
            f"Seeds for target {target} already exist in {out_dir}. Skipped seed collection."
        )
        return out_dir

    llc_command = LLCCommand(target=target, global_isel=global_isel)

    for test in get_runnable_llc_tests(
        backend=target.backend,
        global_isel=global_isel,
        target_filter=create_target_filter(target, props_to_match),
    ):
        if symlink_to_ll and validate_seed(test.path, llc_command, timeout_secs):
            out_dir.joinpath(test.path.name).symlink_to(test.path.absolute())

        if dump_bc:
            bc_path = test.dump_bc(out_dir)

            if not validate_seed(bc_path, llc_command, timeout_secs):
                bc_path.unlink(missing_ok=True)

    print(f"{count_files(out_dir)} seeds written to {out_dir}.")

    return out_dir


def main() -> None:
    args = Args(underscores_to_dashes=True).parse_args()

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
        timeout_secs=args.timeout,
    )


if __name__ == "__main__":
    main()
