from itertools import groupby
from pathlib import Path
from typing import List, Optional
import pandas as pd
from tap import Tap

from llc_test_parsing import LLCTest, parse_llc_tests


class Args(Tap):
    input: str = "llvm-project"
    """root of llvm-project repository"""

    summary_out: Optional[str] = None
    """directory for storing summary (will create if not exist)"""

    seeds_out: Optional[str] = None
    """directory for storing seeds (will create if not exist)"""

    def configure(self):
        self.add_argument("-i", "--input")


def classify(
    arch: str,
    tests: List[LLCTest],
    summary_out: Optional[Path] = None,
    seeds_out: Optional[Path] = None,
) -> None:
    commands = (cmd for test in tests for cmd in test.runnable_llc_commands)

    df = pd.DataFrame(
        columns=["arch", "gisel", "triple", "cpu", "attrs"],
        data=(
            [
                cmd.arch_with_sub,
                cmd.global_isel,
                cmd.triple,
                cmd.cpu,
                ",".join(sorted(cmd.attrs)),
            ]
            for cmd in commands
        ),
    )

    if summary_out:
        df.to_csv(summary_out.joinpath(f"{arch}-raw.csv"))

        df.groupby(
            ["arch", "gisel", "triple", "cpu", "attrs"], dropna=False
        ).size().to_csv(summary_out.joinpath(f"{arch}-summary.csv"))

    for subarch in df["arch"].unique():
        if summary_out:
            subarch_df = df[df["arch"] == subarch]

            pd.crosstab(
                index=subarch_df["cpu"].fillna(""),
                columns=subarch_df["attrs"],
                dropna=False,
            ).to_csv(summary_out.joinpath(f"{subarch}-crosstab.csv"))

        if seeds_out:
            subarch_seeds_dir = seeds_out.joinpath(subarch)
            subarch_seeds_dir.mkdir(exist_ok=True)
            dagisel_seeds_dir = subarch_seeds_dir.joinpath("dagisel")
            dagisel_seeds_dir.mkdir(exist_ok=True)
            gisel_seeds_dir = subarch_seeds_dir.joinpath("gisel")
            gisel_seeds_dir.mkdir(exist_ok=True)

            for test in tests:
                try:
                    if any(
                        cmd.arch_with_sub == subarch
                        for cmd in test.runnable_llc_commands
                    ):
                        if any(
                            not cmd.global_isel for cmd in test.runnable_llc_commands
                        ):
                            dagisel_seeds_dir.joinpath(test.path.name).symlink_to(
                                test.path.absolute()
                            )
                            test.dump_bc(dagisel_seeds_dir)
                        if any(cmd.global_isel for cmd in test.runnable_llc_commands):
                            gisel_seeds_dir.joinpath(test.path.name).symlink_to(
                                test.path.absolute()
                            )
                            test.dump_bc(gisel_seeds_dir)
                except FileExistsError as err:
                    print(err)


def main() -> None:
    args = Args(underscores_to_dashes=True).parse_args()

    summary_out = Path(args.summary_out) if args.summary_out else None
    seeds_out = Path(args.seeds_out) if args.seeds_out else None

    if summary_out:
        summary_out.mkdir(exist_ok=True)

    if seeds_out:
        seeds_out.mkdir(exist_ok=True)

    tests = parse_llc_tests(Path(args.input))

    for key, group in groupby(tests, key=lambda test: test.arch):
        arch_summary_out = summary_out.joinpath(key) if summary_out else None

        if arch_summary_out:
            arch_summary_out.mkdir(exist_ok=True)

        classify(
            arch=key,
            tests=list(group),
            summary_out=arch_summary_out,
            seeds_out=seeds_out,
        )


if __name__ == "__main__":
    main()
