from tap import Tap
from process_data import iterate_over_all_experiments


class Args(Tap):
    input: str
    """root directory containing fuzzing output"""

    def configure(self) -> None:
        self.add_argument("input")


def print_experiment_statuses(root_dir: str) -> None:
    for expr, df in iterate_over_all_experiments(root_dir):
        print(
            expr.isel.ljust(8), expr.arch.ljust(40), str(expr.expr_id).ljust(2), end=" "
        )

        if df.shape[0] > 0:
            print(
                f"{df.iloc[-1]['# relative_time'] / 3600 :.1f}h".ljust(6),
                f"{df.iloc[0]['shw_cvg']:.3%}".ljust(7),
                "->",
                f"{df.iloc[-1]['shw_cvg']:.3%}".ljust(8),
            )
        else:
            print()


def main():
    args = Args().parse_args()
    print_experiment_statuses(args.input)


if __name__ == "__main__":
    main()
