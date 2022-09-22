# python3 scripts/classify.py  --cmd "./llvm-project/build-release/bin/llc -mtriple=r600 /home/peter/aflplusplus-isel/fuzzing/r600/default/crashes/id:004631,sig:06,src:031628+004260,time:323922854,execs:53657883,op:libAFLCustomIRMutator.so,pos:0" --input a
import argparse
import subprocess


class StackTrace:
    def __init__(self, trace):
        self.trace = []
        for line in trace:
            removed_hash_line = line.split("#")[1]
            words = removed_hash_line.split(" ")
            function = words[2]
            location = " ".join(words[3:])

    def __str__(self) -> str:
        ret = ""
        for line in self.trace:
            ret += f"\t{line}\n"
        return ret


class ErrorHead:
    def __init__(self, lines):
        self.msg = ""
        if len(lines) == 0:
            self.err = ""
        else:
            self.err = lines[0]
            if "Cannot select" in self.err:
                self.err = "LLVM ERROR: Cannot select:" + \
                    lines[0].split(" = ")[:6]
            if len(lines) != 1:
                self.msg = lines[1:]


class Report:
    def __init__(self, lines, returncode):
        self.returncode = returncode
        head = []
        trace = []
        running_pass = ""
        is_head = True
        for i in range(len(lines)):
            line = lines[i]
            if is_head:
                head.append(line)
            elif "PLEASE submit a bug report to https://github.com/llvm/llvm-project/issues/ and include the crash backtrace." in line or "Stack dump:" in line or "0.	Program arguments" in line:
                is_head = False
                continue
            elif "#" in line:
                trace.append(line)
            elif ".	Running pass " in line:
                running_pass = line[3:]
            else:
                assert False, "UNREACHABLE, got line: %s" % line
        self.error_head = ErrorHead(head)
        self.stack_trace = StackTrace(trace)

    def __str__(self) -> str:
        return f"Return code: {self.returncode}\nError head: {self.error_head.err}\nTrace:\n {self.stack_trace}"


def run_with_file(path):
    pass


def main():
    parser = argparse.ArgumentParser(
        description='Run all crashed cases and classify them')
    parser.add_argument('--cmd', type=str, required=True,
                        help='The command to run all the files')
    parser.add_argument('--input', type=str, required=True,
                        help='The directory to store all files')

    args = parser.parse_args()
    result = subprocess.run(args.cmd.split(
        ' '), stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    report = Report(
        result.stderr.decode("utf-8").split("\n"),
        result.returncode
    )
    print(report)


main()
