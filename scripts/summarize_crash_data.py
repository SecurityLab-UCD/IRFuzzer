from typing import Generator
import pandas as pd
import os
from common import *
import argparse


def iterate_over_all_experiments(
    dir: str,
) -> Generator[Tuple[ExprimentInfo, int], None, None]:
    for expr_info in for_all_expriments(dir):
        with open(
            os.path.join(expr_info.to_expr_path(), "unique_crashes"), "r"
        ) as file:
            yield (expr_info, int(file.readline()))


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

    df = pd.DataFrame(
        columns=["fuzzer", "isel", "arch", "replicate", "n_unique_crashes"],
        data=(
            [
                exp.fuzzer,
                exp.isel,
                exp.arch,
                exp.expr_id,
                n_unique_crashes,
            ]
            for (exp, n_unique_crashes) in iterate_over_all_experiments(args.input)
        ),
    )

    df_summary = (
        df.drop(columns=["replicate"])
        .groupby(["fuzzer", "isel", "arch"])
        .agg(["min", "max", "count", "mean", "std"])
    )

    df_summary.to_csv("crash_counts.csv")


if __name__ == "__main__":
    main()
