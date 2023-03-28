import argparse
import subprocess
import os
import re
from typing import Iterable, Iterator, List, Optional, Set, Tuple
import shutil
from pathlib import Path
import tempfile

from lib.process_concurrency import run_concurrent_subprocesses


class StackTrace:
    # using tuple instead of list for easier equality check
    stack_frames: Tuple[Tuple[str, str], ...]

    def __init__(self, stacktrace: Iterable[str], remove_addr: bool = False):
        stack_frames: List[Tuple[str, str]] = []

        for line in stacktrace:
            words = line.strip().split(" ")
            assert words[0].startswith("#")
            function = " ".join(words[2:-1])
            location = words[-1]
            if remove_addr:
                location = re.sub(r"0x[0-9a-f]+", "0x_", location)
            stack_frames.append((function, location))

        self.stack_frames = tuple(stack_frames)

    def __str__(self) -> str:
        ret = ""
        for (f, l) in self.stack_frames:
            ret += f"\t{f} {l}\n"
        return ret

    def __len__(self) -> int:
        return len(self.stack_frames)

    def __eq__(self, other) -> bool:
        return self.stack_frames == other.stack_frames

    def __hash__(self) -> int:
        return hash(self.stack_frames)


class CrashError:
    return_code: int
    failed_pass: Optional[str]
    message_raw: str
    message_minimized: str
    type: str
    subtype: Optional[str]
    undefined_external_symbol: bool
    stack_trace: StackTrace
    hash_stacktrace_only: bool
    hash_op_code_only_for_isel_crash: bool

    def __init__(
        self,
        args: List[str],
        return_code: int,
        stderr_iter: Iterator[str],
        hash_stacktrace_only: bool = False,
        hash_op_code_only_for_isel_crash: bool = False,
        remove_addr_in_stacktrace: bool = False,
    ):
        self.return_code = return_code
        self.hash_stacktrace_only = hash_stacktrace_only
        self.hash_op_code_only_for_isel_crash = hash_op_code_only_for_isel_crash
        self.undefined_external_symbol = False

        # extract and minimize error message
        message_lines = []
        while (
            (curr_line := next(stderr_iter, None))
            and curr_line
            != "PLEASE submit a bug report to https://github.com/llvm/llvm-project/issues/ and include the crash backtrace.\n"
        ):
            # do not include the entire DAG in the error message
            if curr_line == "\n" or re.match(r"^ +0x[0-9a-f]+: .+ = .+\n$", curr_line):
                continue

            if re.match(r'LLVM ERROR: Undefined external symbol ".+"\n', curr_line):
                self.undefined_external_symbol = True

            message_lines.append(curr_line)

        self.message_raw = "".join(message_lines)

        self.message_minimized = (
            re.sub(r"%[0-9]+", "%_", self.message_raw)
            .replace(args[0], os.path.basename(args[0]))
            .replace(args[-1], "ir.bc")
        )

        self.message_minimized = re.sub(r"0x[0-9a-f]+", "0x_", self.message_minimized)
        self.message_minimized = re.sub(
            r"(unable to allocate function argument #)[0-9]+",
            r"\1_",
            self.message_minimized,
        )
        self.message_minimized = re.sub(
            r"(Error while trying to spill )(.+)( from class )(.+)(: Cannot scavenge register without an emergency spill slot!)",
            r"\1_\3\4\5",
            self.message_minimized,
        )

        # extract failed pass and stack trace
        self.failed_pass = None
        if (curr_line := next(stderr_iter, None)) and curr_line == "Stack dump:\n":
            # extract failed pass
            while (
                curr_line := next(stderr_iter, None)
            ) and "llvm::sys::PrintStackTrace" not in curr_line:
                if (
                    match := re.match(
                        r" *[0-9]+\.\tRunning pass \'([A-Za-z0-9 ]+)\'", curr_line
                    )
                ) is not None:
                    self.failed_pass = match.group(1)

            # extract stack trace
            try:
                self.stack_trace = StackTrace(stderr_iter, remove_addr_in_stacktrace)
            except:
                print(f"WARNING: Unable to parse stack trace for {args[-1]}")
                self.stack_trace = StackTrace([])
        else:
            self.stack_trace = StackTrace([])

        # determine error type
        if self.message_raw.startswith("LLVM ERROR: unable to legalize instruction:"):
            self.type = "instruction-legalization"
            matches = re.findall(r"G_[A-Z_]+", message_lines[0])
            assert len(matches) == 1
            self.subtype = matches[0]
        elif self.message_raw.startswith("LLVM ERROR: cannot select:"):
            self.type = "global-instruction-selection"
            matches = re.findall(r"G_[A-Z_]+", message_lines[0])
            assert len(matches) == 1
            self.subtype = matches[0]
        elif self.message_raw.startswith("LLVM ERROR: Cannot select:"):
            self.type = "dag-instruction-selection"
            match = re.match(
                r"LLVM ERROR: Cannot select:.+ = ([a-zA-Z0-9_:]+(<.+>)?)",
                message_lines[0],
            )
            if match is None:
                print(f'ERROR: failed to extract instruction from "{message_lines[0]}"')
                self.subtype = "Unknown"
            else:
                self.subtype = match.group(1).split("<")[0]
        else:
            if self.failed_pass is None:
                self.type = "other"
            else:
                self.type = self.failed_pass.lower().replace(" ", "-")
            self.subtype = None

    def __str__(self) -> str:
        return "\n".join(
            [
                f"Return Code: {self.return_code}",
                f"Error Type: {self.type}",
                f"Failed Pass: {self.failed_pass}",
                "Minimized Message:",
                self.message_minimized,
                "Stack Trace:",
                str(self.stack_trace),
            ]
        )

    def get_folder_name(self) -> str:
        return os.path.join(
            self.type,
            self.subtype if self.subtype is not None else "",
            f"tracedepth_{len(self.stack_trace)}__hash_0x{hash(self):08x}",
        )

    def __hash__(self):
        if self.hash_op_code_only_for_isel_crash and (
            self.type == "dag-instruction-selection"
            or self.type == "global-instruction-selection"
        ):
            return hash(self.subtype)

        if self.hash_stacktrace_only:
            return hash(self.stack_trace)

        return hash(self.stack_trace) ^ hash(self.message_minimized)


def classify(
    cmd: List[str],
    input_dir: str | Path,
    output_dir: str | Path,
    force: bool,
    verbose: bool = False,
    create_symlink_to_source: bool = True,
    hash_stacktrace_only: bool = False,
    hash_op_code_only_for_isel_crash: bool = False,
    remove_addr_in_stacktrace: bool = False,
    ignore_undefined_external_symbol: bool = False,
) -> None:
    output_dir = os.path.abspath(output_dir)
    input_dir = os.path.abspath(input_dir)
    temp_dir = tempfile.gettempdir()

    if os.path.exists(output_dir):
        if force:
            shutil.rmtree(output_dir)
        else:
            print(f"{output_dir} already exists, use -f to remove it. Abort.")
            exit(1)

    Path(output_dir).mkdir(parents=True)

    crash_hashes: Set[int] = set()
    false_alarms: List[str] = []

    def on_process_exit(file_name: str, exit_code: Optional[int], p: subprocess.Popen) -> None:
        ir_bc_path: str = p.args[-1]  # type: ignore
        stderr_dump_path = os.path.join(temp_dir, file_name + ".stderr")
        stderr_dump_file = open(stderr_dump_path)

        if os.stat(stderr_dump_path).st_size == 0:
            false_alarms.append(ir_bc_path)
            return

        crash = CrashError(
            p.args,  # type: ignore
            p.returncode,
            stderr_dump_file,
            hash_stacktrace_only,
            hash_op_code_only_for_isel_crash,
            remove_addr_in_stacktrace,
        )

        stderr_dump_file.close()
        os.remove(stderr_dump_path)

        if ignore_undefined_external_symbol and crash.undefined_external_symbol:
            return

        folder_name = crash.get_folder_name()
        folder_path = os.path.join(output_dir, folder_name)
        Path(folder_path).mkdir(parents=True, exist_ok=True)

        if hash(crash) not in crash_hashes:
            crash_hashes.add(hash(crash))
            with open(
                os.path.join(output_dir, folder_name + ".log"), "w+"
            ) as report_path:
                print(crash, file=report_path)

            if verbose:
                print("New crash type:", folder_name)

        if create_symlink_to_source:
            os.symlink(
                ir_bc_path,
                os.path.join(folder_path, os.path.basename(ir_bc_path) + ".bc"),
            )

    run_concurrent_subprocesses(
        iter=list(
            filter(
                lambda file_name: file_name.split(".")[-1] not in ["md", "txt", "s"],
                os.listdir(input_dir),
            )
        ),
        subprocess_creator=lambda file_name: subprocess.Popen(
            cmd + [os.path.join(input_dir, file_name)],
            stdout=subprocess.DEVNULL,
            stderr=open(os.path.join(temp_dir, file_name + ".stderr"), "w"),
        ),
        on_exit=on_process_exit,
    )

    print(f"{len(false_alarms)} false positives, {len(crash_hashes)} unique crashes")
    with open(os.path.join(output_dir, "false_positives.txt"), "a+") as file:
        file.writelines(line + "\n" for line in false_alarms)

    with open(os.path.join(output_dir, "unique_crashes"), "w+") as file:
        file.write(str(len(crash_hashes)))


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Run all crashed cases and classify them"
    )
    parser.add_argument(
        "--cmd",
        type=str,
        required=True,
        help="The command to run on all files in the input dir",
    )
    parser.add_argument(
        "--input", type=str, required=True, help="The directory containing input files"
    )
    parser.add_argument(
        "--output",
        type=str,
        required=False,
        default="output",
        help="The directory to store all organized output",
    )
    parser.add_argument(
        "-f",
        "--force",
        action="store_true",
        help="force delete the output directory if it already exists.",
    )
    args = parser.parse_args()
    classify(args.cmd.split(" "), args.input, args.output, args.force, verbose=True)


if __name__ == "__main__":
    main()
