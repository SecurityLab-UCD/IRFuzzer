import argparse
import os
from typing import Iterable, Iterator, List, Tuple
from matplotlib import pyplot
import numpy as np
import pandas as pd

from process_data import read_experiment_data


def interpolate_data(
    df: pd.DataFrame, x_col: str, y_col: str, desired_xs: Iterator[int]
) -> Iterable[Tuple[int, int]]:
    desired_x = next(desired_xs)
    prev_row = None

    for _, curr_row in df.iterrows():
        curr_x, curr_y = curr_row[x_col], curr_row[y_col]

        if desired_x is None:
            return

        if curr_x == desired_x:
            # no need for interpolation
            yield (desired_x, curr_y)
            desired_x = next(desired_xs, None)
        elif curr_x > desired_x:
            # linear interpolation
            if prev_row is None:
                raise Exception("Not supported yet")

            prev_x, prev_y = prev_row[x_col], prev_row[y_col]
            slope = (curr_y - prev_y) / (curr_x - prev_x)
            yield (desired_x, prev_y + (desired_x - prev_x) * slope)
            desired_x = next(desired_xs, None)

        prev_row = curr_row


def interpolate_data_multiple(
    dfs: Iterable[pd.DataFrame], x_col: str, y_col: str, desired_xs: range
):
    return pd.DataFrame(
        {
            x_col: desired_xs,
            **dict(
                (
                    f"{y_col}_{idx}",
                    [
                        y
                        for _, y in interpolate_data(df, x_col, y_col, iter(desired_xs))
                    ],
                )
                for idx, df in enumerate(dfs)
            ),
        }
    )


def get_confidence_intervals(
    df: pd.DataFrame, x_col: str, summary_col_prefix: str, t: float
) -> pd.DataFrame:
    df_temp = df.drop(columns=[x_col])

    n = df_temp.count(axis=1)
    mean = df_temp.mean(axis=1)
    std_dev = df_temp.std(axis=1)
    std_err = std_dev / np.sqrt(n)

    return pd.DataFrame(
        {
            x_col: df[x_col],
            f"{summary_col_prefix}_ci_lower": mean - t * std_err,
            f"{summary_col_prefix}_mean": mean,
            f"{summary_col_prefix}_ci_upper": mean + t * std_err,
        }
    )


def iterate_plot_data_for_replicates(
    dir: str, n_replicate: int
) -> Iterable[pd.DataFrame]:
    return (
        read_experiment_data(
            os.path.join(
                dir,
                str(i),
                "default/plot_data",
            )
        )
        for i in range(n_replicate)
    )


def compare(
    dir_mt_off: str,
    dir_mt_on: str,
    n_replicate: int,
    x_col: str,
    y_col: str,
    desired_xs: range,
    t: float,
) -> pd.DataFrame:
    df_off = interpolate_data_multiple(
        dfs=iterate_plot_data_for_replicates(dir_mt_off, n_replicate),
        x_col=x_col,
        y_col=y_col,
        desired_xs=desired_xs,
    )

    df_on = interpolate_data_multiple(
        dfs=iterate_plot_data_for_replicates(dir_mt_on, n_replicate),
        x_col=x_col,
        y_col=y_col,
        desired_xs=desired_xs,
    )

    df_off_ci = get_confidence_intervals(df_off, x_col, y_col, t)
    df_on_ci = get_confidence_intervals(df_on, x_col, y_col, t)

    return pd.merge(left=df_off_ci, right=df_on_ci, on=x_col, suffixes=("_off", "_on"))


def main():
    parser = argparse.ArgumentParser(
        description="Compare matcher table coverage of experiments",
    )

    parser.add_argument(
        "-off",
        "--dir-mt-off",
        type=str,
        required=True,
        help="The dir of fuzzing results with matcher table on",
    )

    parser.add_argument(
        "-on",
        "--dir-mt-on",
        type=str,
        required=True,
        help="The dir of fuzzing results with matcher table on",
    )

    parser.add_argument(
        "-o",
        "--out",
        type=str,
        default="compare-all.png",
        help="The path to the figure to be saved",
    )

    args = parser.parse_args()

    x_col = "# relative_time"
    y_col = "shw_cvg"
    desired_xs = range(800, 80000 + 1, 200)
    t = 2.776  # t(df=4, two-tail alpha=0.05)
    dir_mt_off = os.path.join(args.dir_mt_off, "aflisel/dagisel")
    dir_mt_on = os.path.join(args.dir_mt_on, "aflisel/dagisel")
    archs = ["aarch64", "arm", "nvptx", "riscv64", "x86_64"]

    fig, axs = pyplot.subplots(
        nrows=1, ncols=len(archs), layout="constrained", figsize=(12, 2.4)
    )

    for i, arch in enumerate(archs):
        df_ci = compare(
            dir_mt_off=os.path.join(dir_mt_off, arch),
            dir_mt_on=os.path.join(dir_mt_on, arch),
            n_replicate=5,
            x_col=x_col,
            y_col=y_col,
            desired_xs=desired_xs,
            t=t,
        )

        axs[i].set_title(arch)

        axs[i].plot(x_col, f"{y_col}_mean_off", data=df_ci, color="#4899dc")
        axs[i].fill_between(
            x=x_col,
            y1=f"{y_col}_ci_lower_off",
            y2=f"{y_col}_ci_upper_off",
            data=df_ci,
            color="#a2ccee",
            alpha=0.5,
        )

        axs[i].plot(x_col, f"{y_col}_mean_on", data=df_ci, color="#f89d49")
        axs[i].fill_between(
            x=x_col,
            y1=f"{y_col}_ci_lower_on",
            y2=f"{y_col}_ci_upper_on",
            data=df_ci,
            color="#fccea7",
            alpha=0.5,
        )

    axs[0].set_ylabel("Matcher Table Coverage")
    axs[len(archs) // 2].set_xlabel("Time (sec)")
    axs[0].legend(
        [
            "Matcher Table Off (Mean)",
            "Matcher Table Off (95% CI)",
            "Matcher Table On (Mean)",
            "Matcher Table On (95% CI)",
        ],
        bbox_to_anchor=(0, 1.25, 6, 0.2),
        loc="lower left",
        mode="expand",
        ncol=4,
    )

    fig.savefig(args.out)


if __name__ == "__main__":
    main()
