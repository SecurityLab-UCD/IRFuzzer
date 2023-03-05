import argparse
import logging
import os
from functools import reduce
from typing import Callable

from lib import IRFUZZER_DATA_ENV
from lib.experiment import Experiment, get_all_experiments
from lib.fs import subdirs_of


class BlackList:
    name: str
    func: Callable[[Experiment, int], bool]

    def __init__(self, name, func):
        self.name = name
        self.func = func

    def ignore(self, expr: Experiment, mapped_id: int):
        do_ignore = self.func(expr, mapped_id)
        if do_ignore:
            print("\t", self.name, "Failed")
        return do_ignore


use_xcore_makeup = BlackList(
    "use_xcore_makeup",
    lambda expr_info, _: "xcore" == expr_info.arch
    and "xcore-makeup" not in expr_info.expr_path,
)
max_five_expr = BlackList("max_five_expr", lambda _, mapped_id: mapped_id > 4)
fuzzed_long_enough = BlackList(
    "fuzzed_long_enough", lambda expr_info, _: expr_info.run_time < 259000
)
ignore_arm64 = BlackList(
    "ignore_arm64", lambda expr_info, mapped_id: "arm64" in expr_info.arch
)

blacklists = [use_xcore_makeup, max_five_expr, fuzzed_long_enough, ignore_arm64]


def merge_subdirs_by_symlink(src: str, dest: str) -> None:
    for archive_dir in subdirs_of(src):
        for expr in get_all_experiments(archive_dir.path):
            symlink_dest_dir = os.path.join(
                dest, expr.fuzzer, expr.isel, str(expr.target)
            )
            os.makedirs(symlink_dest_dir, exist_ok=True)
            mapped_id = 1 + max(
                [
                    -1,
                    *(
                        int(dir_entry.name)
                        for dir_entry in subdirs_of(symlink_dest_dir)
                    ),
                ]
            )
            symlink_src = expr.path
            symlink_dest = os.path.join(symlink_dest_dir, str(mapped_id))
            print(
                symlink_dest,
                " -> ",
                symlink_src,
                flush=True,
            )

            if reduce(
                lambda a, b: a or b,
                [bl.ignore(expr, mapped_id) for bl in blacklists],
            ):
                print("NOT USED", flush=True)
            else:
                os.symlink(symlink_src, symlink_dest)
                print("DONE", flush=True)


def main() -> None:
    parser = argparse.ArgumentParser(description="Combine experiments into one root.")
    parser.add_argument(
        "-i",
        "--input",
        type=str,
        default="",
        help=f"The directory containing all inputs. Default to ${IRFUZZER_DATA_ENV}",
    )
    args = parser.parse_args()
    if args.input == "":
        args.input = os.getenv(IRFUZZER_DATA_ENV)
        if args.input == None:
            logging.error(
                f"Input directory not set, set --input or {IRFUZZER_DATA_ENV}"
            )
            exit(1)
    # make sure current working directory is archive before running this
    merge_subdirs_by_symlink(args.input, os.path.join(args.input, "../combined"))


if __name__ == "__main__":
    logging.basicConfig()
    logging.getLogger().setLevel(logging.INFO)
    main()
