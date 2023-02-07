from pathlib import Path
import re
import subprocess
from typing import Callable, Iterable, List, Optional, Set


class LLCCommand:
    arch_with_sub: str
    cpu: Optional[str]
    triple: Optional[str]
    attrs: Set[str]
    global_isel: bool

    def __init__(self, command: str, default_triple: Optional[str] = None) -> None:
        assert "llc" in command

        if (match := re.match(r".*-mtriple[= ]\"?([a-z0-9_-]+)", command)) is not None:
            self.triple = match.group(1)
        else:
            self.triple = default_triple

        if (match := re.match(r".*-march[= ]\"?([a-z0-9_-]+)", command)) is not None:
            self.arch_with_sub = match.group(1)
        elif self.triple is not None:
            self.arch_with_sub = self.triple.split("-")[0]
        else:
            raise Exception(f"Failed to determine arch")

        assert self.triple is not None or self.arch_with_sub is not None, f"FATAL"

        if (match := re.match(r".*-mcpu[= ]\"?([a-z0-9-]+)", command)) is not None:
            self.cpu = match.group(1)
        else:
            self.cpu = None

        self.attrs = set(
            ("+" + attr) if not attr.startswith(("+", "-")) else attr
            for arg_val in re.findall(r"-mattr[= ]\"?([A-Za-z0-9,\+-]+)", command)
            for attr in arg_val.split(",")
        )

        assert all(
            attr.startswith(("+", "-")) for attr in self.attrs
        ), f"Invalid attribute"

        self.global_isel = re.match(r".*-global-isel", command) is not None


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

    def dump_bc(self, out_dir: Path) -> None:
        out_path = out_dir.joinpath(self.path.name.removesuffix(".ll") + ".bc")

        process = subprocess.run(
            [
                "./llvm-project/build-release/bin/llvm-as",
                self.path,
                "-o",
                out_path,
            ]
        )

        if process.returncode != 0:
            print(f"WARNING: failed to convert {self.path} to {out_path}")


def parse_llc_tests(
    llvm_root: Path = Path("llvm-project"),
    arch_filter: Callable[[str], bool] = lambda _: True,
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
