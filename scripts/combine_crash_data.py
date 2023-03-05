import argparse
from itertools import groupby
from typing import Generator, Set, Tuple

import pandas as pd

from lib.experiment import Experiment, get_all_experiments
from lib.fs import subdirs_of


def iterate_over_all_experiments(
    dir: str,
) -> Generator[Tuple[Experiment, Set[str]], None, None]:
    for expr in get_all_experiments(dir):
        crashes = set()

        for crash_type_dir in subdirs_of(expr.path):
            for subdir in subdirs_of(crash_type_dir.path):
                if subdir.name.startswith("tracedepth_"):
                    crashes.add(subdir.name)
                else:
                    for subsubdir in subdirs_of(subdir.path):
                        assert subsubdir.name.startswith("tracedepth_")
                        crashes.add(subsubdir.name)

        yield (expr, crashes)


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Combine crash data from differnt experiments to count unique crashes"
    )

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
        key=lambda tuple: ([tuple[0].fuzzer, tuple[0].isel, str(tuple[0].target)]),
    )

    df = pd.DataFrame(
        columns=["fuzzer", "isel", "target", "n_unique_crashes"],
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
