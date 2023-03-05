from typing import Generator, Tuple
import pandas as pd
import argparse

from lib.experiment import Experiment, get_all_experiments


def iterate_over_all_experiments(
    dir: str,
) -> Generator[Tuple[Experiment, int], None, None]:
    for expr_info in get_all_experiments(dir):
        with open(
            expr_info.path.joinpath("unique_crashes"), "r"
        ) as file:
            yield (expr_info, int(file.readline()))


def collect_crash_data(dir: str) -> pd.DataFrame:
    return pd.DataFrame(
        columns=["fuzzer", "isel", "target", "replicate", "n_unique_crashes"],
        data=(
            [
                exp.fuzzer,
                exp.isel,
                str(exp.target),
                exp.replicate_id,
                n_unique_crashes,
            ]
            for (exp, n_unique_crashes) in iterate_over_all_experiments(dir)
        ),
    )


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

    df = collect_crash_data(args.input)

    df_summary = (
        df.drop(columns=["replicate"])
        .groupby(["fuzzer", "isel", "target"])
        .agg(["min", "max", "count", "mean", "std"])
    )

    df_summary.to_csv("crash-counts.csv")

    df_irfuzzer = df[df["fuzzer"] == "irfuzzer"].drop(columns=["fuzzer"])
    df_libfuzzer = df[df["fuzzer"] == "libfuzzer"].drop(columns=["fuzzer"])

    df_comparison = df_irfuzzer.merge(
        df_libfuzzer,
        on=["isel", "target", "replicate"],
        suffixes=("_irfuzzer", "_libfuzzer"),
    )

    df_comparison.to_csv("crash-data-comparison.csv")


if __name__ == "__main__":
    main()
