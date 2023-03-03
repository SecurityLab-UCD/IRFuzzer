from pathlib import Path
from typing import Iterable, Tuple
import pandas as pd
from matplotlib import pyplot
import os
import argparse
import logging
import subprocess

from lib import IRFUZZER_DATA_ENV
from lib.experiment import Experiment, get_all_experiments


def iterate_over_all_experiments(
    dir: Path | str, allow_missing_data: bool = False
) -> Iterable[Tuple[Experiment, pd.DataFrame]]:
    for expr in get_all_experiments(dir):
        try:
            yield (expr, expr.read_plot_data())
        except FileNotFoundError:
            if not allow_missing_data:
                raise


def combine_last_row_of_each_experiment_data(
    experiments: Iterable[Tuple[Experiment, pd.DataFrame]], columns: list[str]
) -> pd.DataFrame:
    return pd.DataFrame(
        columns=["fuzzer", "isel", "target", "replicate", *columns],
        data=(
            [
                exp.fuzzer,
                exp.isel,
                str(exp.target),
                exp.replicate_id,
                *df.tail(1)[columns].values.flatten().tolist(),
            ]
            for (exp, df) in experiments
        ),
    )


def generate_plots(
    experiments: Iterable[Tuple[Experiment, pd.DataFrame]], dir_out: str
) -> None:
    pyplot.ioff()

    for (experiment, df) in experiments:
        figure_path = os.path.join(
            dir_out,
            experiment.fuzzer,
            experiment.isel,
            str(experiment.target),
            str(experiment.replicate_id),
        )
        os.makedirs(figure_path, exist_ok=True)

        try:
            df.plot(x="total_execs", y="saved_crashes").figure.savefig(
                os.path.join(figure_path, "crashes-vs-execs.png")
            )
            df.plot(x="total_execs", y="shw_cvg").figure.savefig(
                os.path.join(figure_path, "shwcvg-vs-execs.png")
            )
            df.plot(x="# relative_time", y="saved_crashes").figure.savefig(
                os.path.join(figure_path, "crashes-vs-time.png")
            )
            df.plot(x="# relative_time", y="shw_cvg").figure.savefig(
                os.path.join(figure_path, "shwcvg-vs-time.png")
            )
        except:
            print(
                f"ERROR: Cannot plot {experiment.fuzzer}/{experiment.isel}/{experiment.target}/{experiment.replicate_id}"
            )

        pyplot.close()


def get_last_col(args):
    df = combine_last_row_of_each_experiment_data(
        iterate_over_all_experiments(args.input, allow_missing_data=True),
        columns=[
            "# relative_time",
            "total_execs",
            "bit_cvg",
            "shw_cvg",
            "corpus_count",
        ],
    )
    outpath = os.path.join(args.output, "last_row_of_each_experiment.csv")
    df.to_csv(outpath, index=False)


def get_summary(args):
    df = combine_last_row_of_each_experiment_data(
        iterate_over_all_experiments(args.input, allow_missing_data=True),
        columns=[
            "# relative_time",
            "total_execs",
            "bit_cvg",
            "shw_cvg",
            "corpus_count",
        ],
    )
    outpath = os.path.join(args.output, "summary.csv")
    df_summary = (
        df.drop(columns=["replicate"])
        .groupby(["fuzzer", "isel", "target"])
        .agg(["min", "max", "count", "mean", "std"])
    )

    df_summary.to_csv(outpath)


def main() -> None:

    parser = argparse.ArgumentParser(description="Process fuzzing output")
    parser.add_argument(
        "-i",
        "--input",
        type=str,
        default="",
        help=f"The directory containing all inputs. Default to ${IRFUZZER_DATA_ENV}",
    )
    parser.add_argument(
        "-o",
        "--output",
        type=str,
        default="./output",
        help="The directory containing processed results, will force removal if it exists.",
    )
    parser.add_argument(
        "-t",
        "--type",
        type=str,
        choices=["LastCol", "Summary", "Plot", "Mann", "Data"],
        required=True,
        help="Type of the job you want me to do.",
    )
    args = parser.parse_args()
    if args.input == "":
        args.input = os.getenv(IRFUZZER_DATA_ENV)
        if args.input == None:
            logging.error(
                f"Input directory not set, set --input or {IRFUZZER_DATA_ENV}"
            )
            exit(1)
    if args.type != "Data":
        if os.path.exists(args.output):
            logging.warning(f"{args.output} exists, removing.")
            subprocess.run(["rm", "-rf", args.output])
        os.mkdir(args.output)

    if args.type == "LastCol":
        get_last_col(args)
    elif args.type == "Summary":
        # TODO: All data required by summary can be found in expr_info now
        # maybe stop reading the whole csv as it is slow.
        get_summary(args)
    elif args.type == "Plot":
        generate_plots(
            experiments=iterate_over_all_experiments(
                args.input, allow_missing_data=True
            ),
            dir_out=args.output,
        )
    elif args.type == "Mann":
        # Mann Whitney U Test to tell if we are statically significant.
        pass


if __name__ == "__main__":
    logging.basicConfig()
    logging.getLogger().setLevel(logging.INFO)
    main()
