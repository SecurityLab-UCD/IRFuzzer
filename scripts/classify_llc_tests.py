from itertools import groupby
from pathlib import Path
import pandas as pd
from tap import Tap

from lib.llc_test import LLCTest, parse_llc_tests


class Args(Tap):
    output: str
    """directory for storing summary (will create if not exist)"""

    def configure(self):
        self.add_argument("-o", "--output")


def classify(
    backend: str,
    tests: list[LLCTest],
    summary_out: Path,
) -> None:
    commands = (cmd for test in tests for cmd in test.runnable_llc_commands)

    df = pd.DataFrame(
        columns=["arch", "gisel", "triple", "cpu", "attrs"],
        data=(
            [
                cmd.target.triple.arch,
                cmd.global_isel,
                str(cmd.target.triple),
                cmd.target.cpu,
                ",".join(sorted(cmd.target.attrs)),
            ]
            for cmd in commands
        ),
    )

    df.to_csv(summary_out.joinpath(f"{backend}-raw.csv"))

    df.groupby(["arch", "gisel", "triple", "cpu", "attrs"], dropna=False).size().to_csv(
        summary_out.joinpath(f"{backend}-summary.csv")
    )

    for arch in df["arch"].unique():
        arch_df = df[df["arch"] == arch]

        pd.crosstab(
            index=arch_df["cpu"].fillna(""),
            columns=arch_df["attrs"],
            dropna=False,
        ).to_csv(summary_out.joinpath(f"{arch}-crosstab.csv"))


def main() -> None:
    args = Args(underscores_to_dashes=True).parse_args()

    summary_out = Path(args.output)
    summary_out.mkdir(exist_ok=True)

    tests = parse_llc_tests()

    for key, group in groupby(tests, key=lambda test: test.backend):
        arch_summary_out = summary_out.joinpath(key)
        arch_summary_out.mkdir(exist_ok=True)

        classify(
            backend=key,
            tests=list(group),
            summary_out=arch_summary_out,
        )


if __name__ == "__main__":
    main()
