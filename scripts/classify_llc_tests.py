from itertools import groupby
from pathlib import Path
import re
from typing import Callable, Iterable, List, Optional
import pandas as pd
from tap import Tap


class Args(Tap):
    input: str = "llvm-project"
    """root of llvm-project repository"""

    output: str
    """directory for storing output (will create if not exist)"""

    def configure(self):
        self.add_argument("-i", "--input")
        self.add_argument("-o", "--output")


class LLCCommand:
    arch: str
    cpu: Optional[str]
    triple: Optional[str]
    attrs: List[str]

    def __init__(self, command: str, default_triple: Optional[str] = None) -> None:
        assert "llc" in command

        if (match := re.match(r".*-mtriple[= ]\"?([a-z0-9-]+)", command)) is not None:
            self.triple = match.group(1)
        else:
            self.triple = default_triple

        if (match := re.match(r".*-march[= ]\"?([a-z0-9-]+)", command)) is not None:
            self.arch = match.group(1)
        elif self.triple is not None:
            self.arch = self.triple.split("-")[0]
        else:
            raise Exception(f"Failed to determine arch")

        assert self.triple is not None or self.arch is not None, f"FATAL"

        if (match := re.match(r".*-mcpu[= ]\"?([a-z0-9-]+)", command)) is not None:
            self.cpu = match.group(1)
        else:
            self.cpu = None

        self.attrs = re.findall(r"-mattr[= ]\"?([a-z0-9-]+)", command)


class LLCTest:
    path: Path

    arch: str

    test_commands: List[str]

    runnable_llc_commands: List[LLCCommand]
    """
    llc commands that can be directly executed without crashing using the test case as an input
    without going through `opt`, `sed`, etc. first.
    """

    code_lines: List[str]

    def __init__(self, arch: str, file_path: Path) -> None:
        assert file_path.name.endswith(".ll")

        self.arch = arch
        self.path = file_path
        self.test_commands = []
        self.code_lines = []

        with open(file_path) as file:
            multiline_command = False  # whether last RUN header ends with '\'
            while line := file.readline():
                if re.match(r".*;.+NOTE:(.+)", line):
                    continue

                match = re.match(r".*;.*RUN:(.+)", line)

                if match is not None:
                    command = match.group(1).strip()

                    if multiline_command:
                        last_command_prev_part = (
                            self.test_commands[-1].removesuffix("\\").strip()
                        )
                        self.test_commands[-1] = f"{last_command_prev_part} {command}"
                    else:
                        self.test_commands.append(command)

                    multiline_command = command.endswith("\\")
                else:
                    assert (
                        not multiline_command
                    ), f"ERROR: something unexpected happened when parsing commands for {file_path}"
                    self.code_lines.append(line)

        assert (
            len(self.test_commands) > 0
        ), f"WARNING: {file_path} does not contain any test command."

        assert (
            len(self.code_lines) > 0
        ), f"WARNING: {file_path} does not contain any test code."

        llc_commands = [cmd for cmd in self.test_commands if "llc " in cmd]
        assert (
            len(llc_commands) > 0
        ), f"WARNING: {file_path} does not contain any `llc` command."

        default_triple = self.get_default_triple()
        runnable_llc_commands_raw = filter(
            lambda cmd: cmd.startswith("llc"),
            (cmd.split("|")[0] for cmd in llc_commands),
        )

        try:
            self.runnable_llc_commands = [
                LLCCommand(cmd, default_triple) for cmd in runnable_llc_commands_raw
            ]
        except Exception as e:
            raise Exception(
                f"ERROR: Failed to parse llc command(s) in {file_path}."
            ) from e

        assert (
            len(self.runnable_llc_commands) > 0
        ), f"WARNING: {file_path} does not contain any runnable `llc` command."

    def get_default_triple(self) -> Optional[str]:
        lines_with_triple = [
            line for line in self.code_lines if line.startswith("target triple")
        ]

        if len(lines_with_triple) == 0:
            return None

        assert (
            len(lines_with_triple) == 1
        ), f"UNEXPECTED: {self.path} has more than one triple specified in code"

        match = re.match(r'target triple ?= ?"([a-z0-9_\.-]+)"', lines_with_triple[0])
        assert (
            match is not None
        ), f"UNEXPECTED: failed to extract triple from '{lines_with_triple[0]}'"

        return match.group(1)


def parse_all_llc_tests(
    llvm_root: Path, arch_filter: Callable[[str], bool] = lambda _: True
) -> Iterable[LLCTest]:
    total = 0
    success = 0

    for arch_dir in llvm_root.joinpath("llvm/test/CodeGen").iterdir():
        if not arch_dir.is_dir() or not arch_filter(arch_dir.name):
            continue

        for file_path in arch_dir.rglob("*.ll"):
            try:
                yield LLCTest(arch_dir.name, file_path)
                success += 1
            except Exception as e:
                print(e)
            total += 1

    print(f"Successfully parsed {success}/{total} LLC tests.")


def main() -> None:

    args = Args().parse_args()

    output_root = Path(args.output)
    output_root.mkdir(exist_ok=True)

    tests = parse_all_llc_tests(Path(args.input))

    for key, group in groupby(tests, key=lambda test: test.arch):
        commands = (cmd for test in group for cmd in test.runnable_llc_commands)

        df = pd.DataFrame(
            columns=["arch", "triple", "cpu", "attrs"],
            data=(
                [
                    cmd.arch,
                    cmd.triple,
                    cmd.cpu,
                    " ".join(sorted(cmd.attrs)),
                ]
                for cmd in commands
            ),
        )

        output_dir = output_root.joinpath(key)
        output_dir.mkdir(exist_ok=True)

        df.to_csv(output_dir.joinpath(f"{key}-raw.csv"))

        df.groupby(["arch", "triple", "cpu", "attrs"], dropna=False).size().to_csv(
            output_dir.joinpath(f"{key}-summary.csv")
        )

        for subarch in df["arch"].unique():
            subarch_df = df[df["arch"] == subarch]

            pd.crosstab(
                index=subarch_df["cpu"].fillna(""),
                columns=subarch_df["attrs"],
                dropna=False,
            ).to_csv(output_dir.joinpath(f"{subarch}-crosstab.csv"))


if __name__ == "__main__":
    main()
