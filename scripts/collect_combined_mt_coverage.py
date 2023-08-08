from itertools import groupby
from typing import Iterable
from bitarray import bitarray
from tap import Tap

from lib.coverage_map import read_coverage_map
from lib.experiment import Experiment, get_all_experiments


class Args(Tap):
    input: str
    """root directory containing fuzzing output"""

    def configure(self) -> None:
        self.add_argument("input")


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


def main():
    args = Args().parse_args()

    for (arch, isel), exprs in groupby(
        get_all_experiments(args.input),
        lambda expr: (expr.target.triple.arch, expr.isel),
    ):
        exprs = list(exprs)
        matcher_table_size = exprs[0].matcher_table_size

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
