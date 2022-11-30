import argparse
from itertools import groupby
from typing import Generator, Set, Tuple

import pandas as pd

from common import ExprimentInfo, for_all_expriments, subdirs_of


def iterate_over_all_experiments(
    dir: str,
) -> Generator[Tuple[ExprimentInfo, Set[str]], None, None]:
    for expr_info in for_all_expriments(dir):
        crashes = set()

        for crash_type_dir in subdirs_of(expr_info.to_expr_path()):
            for subdir in subdirs_of(crash_type_dir.path):
                if subdir.name.startswith("tracedepth_"):
                    crashes.add(subdir.name)
                else:
                    for subsubdir in subdirs_of(subdir.path):
                        assert subsubdir.name.startswith("tracedepth_")
                        crashes.add(subsubdir.name)

        yield (expr_info, crashes)


def main() -> None:
    parser = argparse.ArgumentParser(description="Summerize crash data")

    parser.add_argument(
        "-i",
        "--input",
        type=str,
        required=True,
        help="The input directory (the output directory for batch classification script)",
    )

    args = parser.parse_args()

    groups = groupby(
        iterate_over_all_experiments(args.input),
        key=lambda tuple: ([tuple[0].fuzzer, tuple[0].isel, tuple[0].arch]),
    )

    df = pd.DataFrame(
        columns=["fuzzer", "isel", "arch", "n_unique_crashes"],
        data=(
            [
                *k,
                len(set((crash for (_, crashes) in v for crash in crashes))),
            ]
            for (k, v) in groups
        ),
    )

    df.to_csv("combined-crash-counts.csv")


if __name__ == "__main__":
    main()
