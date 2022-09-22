# python3 scripts/classify.py  --cmd "./llvm-project/build-release/bin/llc -mtriple=r600 /home/peter/aflplusplus-isel/fuzzing/r600/default/crashes/id:004631,sig:06,src:031628+004260,time:323922854,execs:53657883,op:libAFLCustomIRMutator.so,pos:0" --input a
import argparse
from inspect import _void
import subprocess
import os


class StackTrace:
    def __init__(self, trace):
        self.trace = []
        for line in trace:
            removed_hash_line = line.split("#")[1]
            words = removed_hash_line.split(" ")
            function = " ".join(words[2:-1])
            location = words[-1]
            self.trace.append((function, location))

    def __str__(self) -> str:
        ret = ""
        for (f, l) in self.trace:
            ret += f"\t{f} {l}\n"
        return ret

    def __len__(self) -> int:
        return len(self.trace)


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


def run_with_file(path):
    pass


def main() -> None:
    parser = argparse.ArgumentParser(
        description='Run all crashed cases and classify them')
    parser.add_argument('--cmd', type=str, required=True,
                        help='The command to run all the files')
    parser.add_argument('--input', type=str, required=True,
                        help='The directory to store all files')
    args = parser.parse_args()

    cmd = args.cmd.split(' ')
    classes = []
    for f in os.listdir(args.input):
        if f == "README.md":
            continue
        path = os.path.join(args.input, f)
        result = subprocess.run(
            cmd + [path], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        report = Report(
            result.stderr.decode("utf-8").split("\n"),
            result.returncode
        )

        if report not in classes:
            classes.append(report)
            print(report.get_folder_name())


main()
