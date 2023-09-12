from functools import reduce
from pathlib import Path
from typing import Callable, Optional
from tap import Tap
import pandas as pd
from scipy.stats import mannwhitneyu

from lib.experiment import Experiment, get_all_experiments
from lib.target import Target


class Args(Tap):
    input: str
    """root directory containing fuzzing output"""

    output: Optional[str] = None
    """
    output file path;
    supported formats: txt, csv, tex;
    if not provided, write to stdout in string format.
    """

    summerize: bool = False
    """if true, print summary of experiments"""

    compare: bool = False
    """comparison among fuzzers"""

    def configure(self) -> None:
        self.add_argument("input")
        self.add_argument("-o", "--output")
        self.add_argument("-s", "--summerize")
        self.add_argument("-c", "--compare")

    def get_output_format(self) -> Optional[str]:
        return None if self.output is None else self.output.split(".")[-1]


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

display_name_map: dict[str, str] = {
    "aflplusplus": "AFL++",
    "libfuzzer": "FM",
    "irfuzzer": "IRF",
    "ir-intrinsic-feedback": "IRF+IF",
    "fuzzer": "Fuzzer",
    "isel": "ISel",
    "target": "Target",
    "triple": "Arch",
    "cpu": "CPU",
    "attrs": "Attrs",
    "mt_size": "Matcher Table Size",
    "replicate": "Replicate",
    "run_time": "Run Time",
    "init_br_cvg": "Initial Branch Coverage",
    "cur_br_cvg": "Branch Coverage",
    "init_mt_cvg": "Initial Matcher Table Coverage",
    "cur_mt_cvg": "Matcher Table Coverage",
}

fuzzer_order: list[str] = [
    "Seeds",
    "aflplusplus",
    "libfuzzer",
    "irfuzzer",
    "ir-intrinsic-feedback",
]

fuzzer_order_map: dict[str, int] = {fuzzer: i for i, fuzzer in enumerate(fuzzer_order)}

fuzzer_sig_test_map: dict[str, list[str]] = {
    "irfuzzer": ["aflplusplus", "libfuzzer"],
    "ir-intrinsic-feedback": ["aflplusplus", "libfuzzer"],
}


def read_experiment_stats(root_dir: str) -> pd.DataFrame:
    return pd.DataFrame(
        columns=list(experiment_prop_map.keys()),
        data=(
            [experiment_prop_map[prop](expr) for prop in experiment_prop_map.keys()]
            for expr in get_all_experiments(root_dir)
        ),
    )


def summerize(df: pd.DataFrame) -> pd.DataFrame:
    # Keep raw data for statistical test

    def raw_data(x):
        return x.values.tolist()

    return df.groupby(["fuzzer", "isel", "target", "mt_size"]).agg(
        {
            "replicate": ["count"],
            "run_time": ["mean"],
            "init_br_cvg": ["mean"],
            "cur_br_cvg": ["mean", "std", raw_data],
            "init_mt_cvg": ["mean"],
            "cur_mt_cvg": ["mean", "std", raw_data],
        }
    )


def compare(df: pd.DataFrame) -> pd.DataFrame:
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

    df = (
        reduce(
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
        )
        .drop(
            # drop columns of NaN values that comes from the initial empty dataframe
            # but those are necessary in the merge process
            columns=[
                ("init_br_cvg", "mean"),
                ("cur_br_cvg", "mean"),
                ("cur_br_cvg", "std"),
                ("cur_br_cvg", "raw_data"),
                ("init_mt_cvg", "mean"),
                ("cur_mt_cvg", "mean"),
                ("cur_mt_cvg", "std"),
                ("cur_mt_cvg", "raw_data"),
            ]
        )
        .reset_index(["mt_size"])
    )

    df.columns = pd.MultiIndex.from_tuples(
        [
            (*col, "")
            if "_cvg_" not in col[0]
            else (col[0].rsplit("_", 1)[0], col[0].rsplit("_", 1)[1], col[1])
            for col in df.columns
        ]
    )

    # calculate p-value
    for col in df.columns:
        if col[0] not in ["cur_mt_cvg", "cur_br_cvg"]:
            continue

        expr_fuzzer = col[1]
        ctrl_fuzzers = fuzzer_sig_test_map.get(expr_fuzzer, [])

        if len(ctrl_fuzzers) == 0:
            continue

        df[col[0], expr_fuzzer, "p-value"] = df.apply(
            lambda row: max(
                float(
                    mannwhitneyu(
                        row[(col[0], expr_fuzzer, "raw_data")],
                        row[(col[0], ctrl_fuzzer, "raw_data")],
                        alternative="greater",
                    ).pvalue
                )
                for ctrl_fuzzer in ctrl_fuzzers
            ),
            axis=1,
        )

    # drop raw data
    df.drop(
        columns=[
            col
            for col in df.columns
            if col[0] in ["cur_mt_cvg", "cur_br_cvg"] and col[2] == "raw_data"
        ],
        inplace=True,
    )

    # sort columns
    df.sort_index(
        axis=1,
        key=lambda index: index.map(
            lambda col: col if col not in fuzzer_order_map else -fuzzer_order_map[col]
        ),
        inplace=True,
        ascending=False,
    )

    return df


def dump_tex(df: pd.DataFrame, out_path: Path) -> None:
    # expand target index into multiindex
    assert df.index.names[-1] == "target"
    targets = {s: Target.parse(s) for s in df.index.get_level_values("target").unique()}
    df.index = pd.MultiIndex.from_tuples(
        tuples=[
            (
                *values[:-1],
                str(targets[values[-1]].triple),
                targets[values[-1]].cpu or "",
                str(targets[values[-1]].attrs),
            )
            for values in df.index.values
        ],
        names=[*df.index.names[:-1], "triple", "cpu", "attrs"],
    )

    # drop index with only 1 unique value
    for level in df.index.names:
        if df.index.get_level_values(level).nunique() == 1:
            df = df.droplevel(level)

    # drop columns with only 1 unique value
    nunique = df.nunique()
    cols_to_drop = nunique[nunique == 1].index
    df.drop(cols_to_drop, axis=1, inplace=True)

    # format columns
    df_numeric = df.copy()
    for col in df.columns:
        if col[2] != "mean":
            continue

        p_value_col = (col[0], col[1], "p-value")

        def format_coverage_for_col(row) -> str:
            if "riscv" in str(row.name) and col[0].endswith("_mt_cvg"):
                return f"{row[col] * 1000 :.1f}\\si{{\\permille}}"
            else:
                return f"{row[col] * 100 :.1f}\\si{{\\percent}}"

        def is_best_col(row) -> bool:
            return row[col] == max(
                row[c] for c in row.index if c[0] == col[0] and c[2] == "mean"
            )

        if p_value_col in df.columns:
            # bold mean values whose p-value is less than 0.05 and is the best in class
            df[col] = df_numeric.apply(
                lambda row: f"\\textbf{{{format_coverage_for_col(row)}}}"
                if row[p_value_col] < 0.05 and is_best_col(row)
                else format_coverage_for_col(row),
                axis=1,
            )
        else:
            df[col] = df_numeric.apply(format_coverage_for_col, axis=1)

    # drop other columns that are not needed
    df.drop(
        [
            col
            for col in df.columns
            if (
                col[0] in ["mt_size"]
                or "std" in col[2]
                or "p-value" in col[2]
                or ("init_" in col[0] and col[1] != "irfuzzer")
            )
        ],
        axis=1,
        inplace=True,
    )

    # drop the bottom level of column index
    df = df.droplevel(2, axis=1)

    # reorganize multiindex on columns
    df.columns = pd.MultiIndex.from_tuples(
        [
            col if "init_" not in col[0] else (col[0].replace("init_", "cur_"), "Seeds")
            for col in df.columns
        ]
    )

    # sort columns
    df.sort_index(
        axis=1,
        key=lambda index: index.map(lambda col: fuzzer_order_map.get(col, col)),
        inplace=True,
    )

    df.style.to_latex(
        out_path,
        column_format="l" * df.index.nlevels + "|" + "r" * len(df.columns),
        hrules=True,
        multicol_align="c",
        multirow_align="t",
    )

    tex = out_path.read_text()

    for from_, to_ in display_name_map.items():
        tex = tex.replace(from_, to_)

    out_path.write_text(
        "\n".join(
            [
                "\\documentclass{article}",
                "\\usepackage[left=0.5cm, right=0.5cm]{geometry}",
                "\\usepackage{booktabs}",
                "\\usepackage{multirow}",
                "\\usepackage{siunitx}",
                "\\DeclareSIUnit\\permille{\\text{\\textperthousand}}"
                "\\begin{document}",
                "",
                tex.replace("_", "\\_"),
                "",
                "\\end{document}",
                "",
            ]
        )
    )


def main():
    args = Args().parse_args()

    df = read_experiment_stats(args.input)

    if args.compare:
        args.summerize = True

    if args.summerize:
        df = summerize(df)

    if args.compare:
        df = compare(df)

    match args.get_output_format():
        case None | "txt":
            print(
                df.to_string(
                    index=args.summerize,
                    formatters={
                        "run_time": lambda sec: f"{sec / 3600 :.1f}h",
                        "init_mt_cvg": "{:,.2%}".format,
                        "cur_mt_cvg": "{:,.2%}".format,
                        "init_br_cvg": "{:,.2%}".format,
                        "cur_br_cvg": "{:,.2%}".format,
                    },
                ),
                file=None if args.output is None else open(args.output, "w"),
            )
        case "csv":
            df.to_csv(args.output, index=args.summerize)
        case "tex":
            if args.output is None:
                raise Exception()

            if not args.compare:
                raise NotImplementedError("Only support comparison for tex output")

            dump_tex(df, Path(args.output))
        case _:
            raise NotImplementedError(
                f"Unsupported output format: {args.get_output_format()}"
            )


if __name__ == "__main__":
    main()
