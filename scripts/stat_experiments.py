from functools import reduce
from typing import Callable, Optional
from tap import Tap
from lib.experiment import Experiment, get_all_experiments
import pandas as pd


class Args(Tap):
    input: str
    """root directory containing fuzzing output"""

    output: Optional[str] = None
    """output csv file to write to; if not provided, write to stdout in string format"""

    summerize: bool = False
    """if true, print summary of experiments"""

    compare: bool = False
    """comparison among fuzzers"""

    def configure(self) -> None:
        self.add_argument("input")
        self.add_argument("-o", "--output")
        self.add_argument("-s", "--summerize")
        self.add_argument("-c", "--compare")


experiment_prop_map: dict[str, Callable[[Experiment], str | int | float | None]] = {
    "fuzzer": lambda expr: expr.fuzzer,
    "isel": lambda expr: expr.isel,
    "target": lambda expr: str(expr.target),
    "mt_size": lambda expr: expr.matcher_table_size,
    "replicate": lambda expr: expr.replicate_id,
    "run_time": lambda expr: expr.run_time,
    "init_br_cvg": lambda expr: expr.initial_branch_coverage,
    "cur_br_cvg": lambda expr: expr.branch_coverage,
    "init_mt_cvg": lambda expr: expr.initial_matcher_table_coverage,
    "cur_mt_cvg": lambda expr: expr.matcher_table_coverage,
}


def read_experiment_stats(root_dir: str) -> pd.DataFrame:
    return pd.DataFrame(
        columns=list(experiment_prop_map.keys()),
        data=(
            [experiment_prop_map[prop](expr) for prop in experiment_prop_map.keys()]
            for expr in get_all_experiments(root_dir)
        ),
    )


def main():
    args = Args().parse_args()

    df = read_experiment_stats(args.input)

    if args.compare:
        args.summerize = True

    if args.summerize:
        df = df.groupby(["fuzzer", "isel", "target", "mt_size"]).agg(
            {
                "replicate": ["count"],
                "run_time": ["mean"],
                "init_br_cvg": ["mean"],
                "cur_br_cvg": ["mean", "std"],
                "init_mt_cvg": ["mean"],
                "cur_mt_cvg": ["mean", "std"],
            }
        )

    if args.compare:
        fuzzers = df.index.get_level_values("fuzzer").unique()

        # df = df.round(decimals=4)

        # round mean run time to nearest second
        # since a few experiments may have 1 second difference than the set time
        df[("run_time", "mean")] = df[("run_time", "mean")].round()


        # The following commented code is a reference for how to merge 2 dataframes
        # the actual code using `reduce` further below is a generalization but harder to read
        #
        # df = pd.merge(
        #     left=df.loc[fuzzers[0]],
        #     right=df.loc[fuzzers[1]],
        #     how="outer",
        #     on=[
        #         "isel",
        #         "target",
        #         ("replicate", "count"),
        #         ("run_time", "mean"),
        #         ("init_mt_cvg", "mean"),
        #     ],
        #     suffixes=(f"_{fuzzers[0]}", f"_{fuzzers[1]}"),
        # )

        df = reduce(
            lambda df_tmp, fuzzer: df_tmp.merge(
                right=df.loc[fuzzer],
                how="outer",
                on=[
                    "isel",
                    "target",
                    "mt_size",
                    ("replicate", "count"),
                    ("run_time", "mean"),
                ],
                suffixes=(None, f"_{fuzzer}"),
            ),

            fuzzers,

            # get a empty dataframe with the same columns and indexes as df
            df.iloc[:0, :],
        ).drop(
            # drop columns of NaN values that comes from the initial empty dataframe
            # but those are necessary in the merge process
            columns=[
                ("init_br_cvg", "mean"),
                ("cur_br_cvg", "mean"),
                ("cur_br_cvg", "std"),
                ("init_mt_cvg", "mean"),
                ("cur_mt_cvg", "mean"),
                ("cur_mt_cvg", "std"),
            ]
        )

    if args.output is None:
        print(
            df.to_string(
                index=args.summerize,
                formatters={
                    "run_time": lambda sec: f"{sec / 3600 :.1f}h",
                    "init_mt_cvg": "{:,.3%}".format,
                    "cur_mt_cvg": "{:,.3%}".format,
                    "init_br_cvg": "{:,.3%}".format,
                    "cur_br_cvg": "{:,.3%}".format,
                },
            )
        )
    else:
        df.to_csv(args.output, index=args.summerize)


if __name__ == "__main__":
    main()
