from typing import Iterable, List, NamedTuple
import pandas as pd
from matplotlib import pyplot
import os
from common import subdirs_of, IRFUZZER_DATA_ENV
import argparse
import logging
import subprocess

def convert_percentage_to_float(s: str) -> float:
    return float(s.strip("%")) / 100


def read_experiment_data(path: str) -> pd.DataFrame:
    # the table header is not consistent, so we don't want pandas to detect and process the header (1st row of csv)
    # but let it use the hard-coded column names below.
    return pd.read_csv(
        path,
        index_col=False,
        header=None,
        skiprows=1,
        names=[
            "# relative_time",
            "cycles_done",
            "cur_item",
            "corpus_count",
            "pending_total",
            "pending_favs",
            "bit_cvg",
            "shw_cvg",
            "saved_crashes",
            "saved_hangs",
            "max_depth",
            "execs_per_sec",
            "total_execs",
            "edges_found",
        ],
        converters={
            "bit_cvg": convert_percentage_to_float,
            "shw_cvg": convert_percentage_to_float,
        },
    )


class Experiment(NamedTuple):
    fuzzer: str
    isel: str
    arch: str
    replicate_id: int
    data: pd.DataFrame


def iterate_over_all_experiments(
    dir: str, allow_missing_data: bool = False
) -> Iterable[Experiment]:
    for fuzzer_dir in subdirs_of(dir):
        fuzzer = fuzzer_dir.name
        for isel_dir in subdirs_of(fuzzer_dir.path):
            isel = isel_dir.name
            for arch_dir in subdirs_of(isel_dir.path):
                arch = arch_dir.name
                for replicate_dir in subdirs_of(arch_dir.path):
                    replicate_id = int(replicate_dir.name)
                    plot_data_path = os.path.join(
                        replicate_dir.path, "default", "plot_data"
                    )
                    try:
                        yield Experiment(
                            fuzzer,
                            isel,
                            arch,
                            replicate_id,
                            read_experiment_data(plot_data_path),
                        )
                    except FileNotFoundError:
                        if not allow_missing_data:
                            raise


def combine_last_row_of_each_experiment_data(
    experiments: Iterable[Experiment], columns: List[str]
) -> pd.DataFrame:
    return pd.DataFrame(
        columns=["fuzzer", "isel", "arch", "replicate", *columns],
        data=(
            [
                exp.fuzzer,
                exp.isel,
                exp.arch,
                exp.replicate_id,
                *exp.data.tail(1)[columns].values.flatten().tolist(),
            ]
            for exp in experiments
        ),
    )


def generate_plots(experiments: Iterable[Experiment], dir_out: str) -> None:
    pyplot.ioff()

    for experiment in experiments:
        figure_path = os.path.join(
            dir_out,
            experiment.fuzzer,
            experiment.isel,
            experiment.arch,
            str(experiment.replicate_id),
        )
        os.makedirs(figure_path, exist_ok=True)

        try:
            experiment.data.plot(x="total_execs", y="saved_crashes").figure.savefig(
                os.path.join(figure_path, "crashes-vs-execs.png")
            )
            experiment.data.plot(x="total_execs", y="shw_cvg").figure.savefig(
                os.path.join(figure_path, "shwcvg-vs-execs.png")
            )
            experiment.data.plot(x="# relative_time", y="saved_crashes").figure.savefig(
                os.path.join(figure_path, "crashes-vs-time.png")
            )
            experiment.data.plot(x="# relative_time", y="shw_cvg").figure.savefig(
                os.path.join(figure_path, "shwcvg-vs-time.png")
            )
        except:
            print(
                f"ERROR: Cannot plot {experiment.fuzzer}/{experiment.isel}/{experiment.arch}/{experiment.replicate_id}"
            )

        pyplot.close()

def get_last_col(args):
    df = combine_last_row_of_each_experiment_data(
        iterate_over_all_experiments(
            args.input, allow_missing_data=True
        ),
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
        iterate_over_all_experiments(
            args.input, allow_missing_data=True
        ),
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
        .groupby(["fuzzer", "isel", "arch"])
        .agg(["min", "max", "count", "mean", "std"])
    )

    df_summary.to_csv(outpath)

def get_data_slice(args):
    if args.data == []:
        logging.error("Data not set, abort")
        exit(1)
    df = combine_last_row_of_each_experiment_data(
        iterate_over_all_experiments(
            args.input, allow_missing_data=True
        ),
        columns=args.data
    )
    df_summary = (df.drop(columns=["replicate"])
        .groupby(["fuzzer", "isel", "arch"])
        .agg(["mean"])
    )
    print(df_summary)
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
    parser.add_argument(
        "-d",
        "--data",
        default=[],
        nargs="+", 
        type=str,
        help="Type of data you want to show."
    )
    
    args = parser.parse_args()
    if args.input=="":
        args.input=os.getenv(IRFUZZER_DATA_ENV)
        if args.input == None:
            logging.error(f"Input directory not set, set --input or {IRFUZZER_DATA_ENV}")
            exit(1)
    if args.type != "Data":
        if os.path.exists(args.output):
            logging.warning(f"{args.output} exists, removing.")
            subprocess.run(["rm", "-rf", args.output])
        os.mkdir(args.output)

    if args.type == "LastCol":
        get_last_col(args)
    elif args.type == "Summary":
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
    elif args.type == "Data":
        get_data_slice(args)


if __name__ == "__main__":
    logging.basicConfig()
    logging.getLogger().setLevel(logging.INFO)
    main()
