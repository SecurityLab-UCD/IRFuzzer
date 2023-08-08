from typing import Callable, Optional
from tap import Tap
from lib.experiment import Experiment, get_all_experiments
import pandas as pd


class Args(Tap):
    input: str
    """root directory containing fuzzing output"""

    output: Optional[str] = None
    """output csv file to write to; if not provided, write to stdout in string format"""

    def configure(self) -> None:
        self.add_argument("input")
        self.add_argument("-o", "--output")


experiment_prop_map: dict[str, Callable[[Experiment], str | int | float | None]] = {
    "fuzzer": lambda expr: expr.fuzzer,
    "isel": lambda expr: expr.isel,
    "target": lambda expr: str(expr.target),
    "replicate": lambda expr: expr.replicate_id,
    "run_time": lambda expr: expr.run_time,
    # "init_br_cvg": lambda expr: expr.initial_bitmap_coverage,
    # "cur_br_cvg": lambda expr: expr.bitmap_coverage,
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

    if args.output is None:
        print(
            df.to_string(
                index=False,
                formatters={
                    "run_time": lambda sec: f"{sec / 3600 :.1f}h",
                    "init_mt_cvg": "{:,.3%}".format,
                    "cur_mt_cvg": "{:,.3%}".format,
                },
            )
        )
    else:
        df.to_csv(args.output, index=False)


if __name__ == "__main__":
    main()
