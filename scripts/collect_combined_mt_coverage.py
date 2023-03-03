from itertools import groupby
from pathlib import Path
from typing import Iterable
from bitarray import bitarray
from tap import Tap
from math import ceil

from lib.arch import ARCH_TO_BACKEND_MAP
from lib.experiment import Experiment, get_all_experiments
from lib.matcher_table_sizes import (
    DAGISEL_MATCHER_TABLE_SIZES,
    GISEL_MATCHER_TABLE_SIZES,
)


class Args(Tap):
    input: str
    """root directory containing fuzzing output"""

    def configure(self) -> None:
        self.add_argument("input")


def read_coverage_map(path: Path, matcher_table_size: int) -> bitarray:
    cvg_map = bitarray()

    with open(path, "rb") as file:
        cvg_map.fromfile(file)
        assert ceil(matcher_table_size / 64) * 64 == len(cvg_map)
        cvg_map = cvg_map[:matcher_table_size]

    return cvg_map


def get_combined_coverage_map(
    experiments: Iterable[Experiment], map_size: int, map_rel_path: str
) -> bitarray:
    combined_cvg_map = bitarray(map_size)
    combined_cvg_map.setall(1)

    for expr in experiments:
        cvg_map_path = expr.path.joinpath(map_rel_path)

        if not cvg_map_path.exists():
            print(f"WARNING: {cvg_map_path} does not exist. Skipped.")
            continue

        combined_cvg_map &= read_coverage_map(cvg_map_path, map_size)

    return combined_cvg_map


def get_matcher_table_size(backend: str, isel: str) -> int:
    backend = ARCH_TO_BACKEND_MAP[backend]

    if isel == "dagisel":
        return DAGISEL_MATCHER_TABLE_SIZES[backend]
    elif isel == "gisel":
        return GISEL_MATCHER_TABLE_SIZES[backend]
    else:
        raise Exception("Invalid ISel")


def main():
    args = Args().parse_args()

    for (arch, isel), exprs in groupby(
        get_all_experiments(args.input),
        lambda expr: (expr.target.triple.arch, expr.isel),
    ):
        matcher_table_size = get_matcher_table_size(arch, isel)
        exprs = list(exprs)

        initial_cvg_map = get_combined_coverage_map(
            exprs,
            matcher_table_size,
            "default/fuzz_initial_shadowmap",
        )

        current_cvg_map = get_combined_coverage_map(
            exprs,
            matcher_table_size,
            "default/fuzz_shadowmap",
        )

        assert len(initial_cvg_map) == len(current_cvg_map)

        print(
            arch.ljust(10),
            isel.ljust(8),
            f"{matcher_table_size}".ljust(8),
            f"{initial_cvg_map.count(0) / matcher_table_size :.3%}".ljust(6),
            "->",
            f"{current_cvg_map.count(0) / matcher_table_size :.3%}".ljust(6),
        )


if __name__ == "__main__":
    main()
