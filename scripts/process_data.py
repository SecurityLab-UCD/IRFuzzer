from typing import Iterable, List, NamedTuple
import pandas as pd
from matplotlib import pyplot
import os
from common import subdirs_of


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


def main() -> None:
    df = combine_last_row_of_each_experiment_data(
        iterate_over_all_experiments(
            "/home/peter/archives/combined", allow_missing_data=True
        ),
        columns=[
            "# relative_time",
            "total_execs",
            "bit_cvg",
            "shw_cvg",
            "corpus_count",
        ],
    )

    df.to_csv("last_row_of_each_experiment.csv", index=False)

    df_summary = (
        df.drop(columns=["replicate"])
        .groupby(["fuzzer", "isel", "arch"])
        .agg(["min", "max", "count", "mean", "std"])
    )

    df_summary.to_csv("summary.csv")

    generate_plots(
        experiments=iterate_over_all_experiments(
            "/home/peter/isel-aflexpr/archive", allow_missing_data=True
        ),
        dir_out="/home/henry/isel-aflexpr-plots",
    )


if __name__ == "__main__":
    main()
