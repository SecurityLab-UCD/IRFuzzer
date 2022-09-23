import argparse
from inspect import _void
import subprocess
import os
from tqdm import tqdm
import shutil


class StackTrace:
    def __init__(self, trace):
        self.trace = []
        for line in trace:
            removed_hash_line = line.split("#")[1]
            words = removed_hash_line.split(" ")
            function = " ".join(words[2:-1])
            location = words[-1]
            self.trace.append((function, location))
        self.trace = tuple(self.trace)

    def __str__(self) -> str:
        ret = ""
        for (f, l) in self.trace:
            ret += f"\t{f} {l}\n"
        return ret

    def __len__(self) -> int:
        return len(self.trace)

    def __eq__(self, other) -> bool:
        return self.trace == other.trace

    def __hash__(self):
        return hash(self.trace)


class ErrorHead:
    def __init__(self, lines):
        self.msg = ""
        if len(lines) == 0:
            self.err = ""
        else:
            self.err = lines[0]
            if "Cannot select" in self.err:
                self.err = "LLVM ERROR: Cannot select: " + \
                    lines[0].split(" = ")[1][:6]
            if len(lines) != 1:
                self.msg = lines[1:]


class Report:
    def __init__(self, lines, returncode):
        self.returncode = returncode
        head = []
        trace = []
        is_head = True
        for i in range(len(lines)):
            line = lines[i]
            if "PLEASE submit a bug report to" in line:
                is_head = False
                continue
            if is_head:
                head.append(line)
            elif '#' in line:
                trace.append(line)
        self.error_head = ErrorHead(head)
        self.stack_trace = StackTrace(trace)

    def get_folder_name(self) -> str:
        return f"tracedepth_{len(self.stack_trace)}__hash_0x{hash(self):08x}"

    def __str__(self) -> str:
        return f"Return code: {self.returncode}\nError head: {self.error_head.err}\nTrace:\n {self.stack_trace}"

    def __eq__(self, other) -> bool:
        return self.error_head.err == other.error_head.err and self.stack_trace == other.stack_trace

    def __hash__(self):
        return hash(self.stack_trace) ^ hash(self.error_head.err)


def main() -> None:
    parser = argparse.ArgumentParser(
        description='Run all crashed cases and classify them')
    parser.add_argument('--cmd', type=str, required=True,
                        help='The command to run all the files')
    parser.add_argument('--input', type=str, required=True,
                        help='The directory to store all files')
    parser.add_argument('--output', type=str, required=False, default="output",
                        help="The directory to store all organized output")
    parser.add_argument('-f', '--force', action='store_true',
                        help="force delete the output directory if it already exists.")
    args = parser.parse_args()

    args.output = os.path.abspath(args.output)
    args.input = os.path.abspath(args.input)

    if os.path.exists(args.output):
        if args.force:
            shutil.rmtree(args.output)
        else:
            print(f"{args.output} already exists, use -f to remove it. Abort.")
            exit(1)
    os.mkdir(args.output)

    cmd = args.cmd.split(' ')
    classes = []
    # TODO: Parallel the for loop.
    for f in tqdm(os.listdir(args.input)):
        if f.endswith('.md') or f.endswith('.txt') or f.endswith('.s'):
            continue
        path = os.path.join(args.input, f)
        result = subprocess.run(
            cmd + [path], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        report = Report(
            result.stderr.decode("utf-8").split("\n"),
            result.returncode
        )

        folder_name = report.get_folder_name()
        folder_path = os.path.join(args.output, folder_name)
        if report not in classes:
            classes.append(report)
            os.mkdir(folder_path)
            with open(os.path.join(args.output, folder_name+".txt"), "w+") as report_path:
                print(report, file=report_path)
            print("New crash:", folder_name)
        os.symlink(path, os.path.join(folder_path, f))


main()
