import logging
from typing import Dict, Iterable, List, Literal, Optional, Tuple
import common
import subprocess
import os
import multiprocessing
from tap import Tap


DOCKER_IMAGE = "irfuzzer"
SECONDS_PER_UNIT = {"s": 1, "m": 60, "h": 3600, "d": 86400, "w": 604800}

Fuzzer = Literal["aflplusplus", "libfuzzer", "irfuzzer"]
Tier = Literal[0, 1, 2, 3]
ClutserType = Literal["screen", "docker", "stdout"]
CpuAttrArchList = List[Tuple[str, str, str]]


class Args(Tap):
    """
    Command-line Arguments
    (Reference: https://github.com/swansonk14/typed-argument-parser)
    """

    fuzzer: Fuzzer = "irfuzzer"
    """the fuzzer used for fuzzing"""

    input: str = "./seeds"
    """the directory containing input seeds"""

    output: str = "./fuzzing"
    """the output directory"""

    on_exist: Literal["abort", "force", "ignore"] = "abort"
    """the action to take if the output directory already exists"""

    isel: Literal["gisel", "dagisel"] = "dagisel"
    """the LLVM instruction selection method to fuzz"""

    tier: Optional[Tier] = None
    """
    the set of targets to fuzz
    (0: everything, 1: Tier 1, 2: Tier 2, see 'common.py' for details)
    (can be overriden by `--set`)
    """

    set: Optional[List[str]] = None
    """manually specify targets to fuzz ('tier' will be ignored)"""

    time: str = "5m"
    """duration for each experiment (e.g. '100s', '30m', '2h', '1d')"""

    repeat: int = 3
    """how many times each experiemt should run"""

    offset: int = 0
    """the offset to start counting experiments"""

    jobs: int = multiprocessing.cpu_count()
    """the max number of concurrent subprocesses"""

    type: ClutserType
    """the method to start fuzzing cluster"""

    def configure(self):
        self.add_argument("-i", "--input")
        self.add_argument("-o", "--output")
        self.add_argument("-j", "--jobs")
        self.add_argument("-t", "--time")

    def get_cpu_attr_arch_list(self) -> CpuAttrArchList:
        if self.tier == 0:
            return [("", "", triple) for triple in common.TRIPLE_ARCH_MAP.keys()]
        elif self.tier == 1:
            return common.CPU_ATTR_ARCH_LIST_TIER_1
        elif self.tier == 2:
            return common.CPU_ATTR_ARCH_LIST_TIER_2
        elif self.tier == 3:
            return common.CPU_ATTR_ARCH_LIST_TIER_3
        elif self.set is not None:
            return [tuple(s.split(" ")) for s in self.set]
        else:
            logging.error("Either '--tier' or '--set' has to be specified.")
            exit(1)

    def get_time_in_seconds(self) -> int:
        return int(self.time[:-1]) * SECONDS_PER_UNIT[self.time[-1]]


def get_export_command(name: str, value: str) -> str:
    return f'export {name}="{value}"'


def get_export_commands(env: Dict[str, str]) -> Iterable[str]:
    return (get_export_command(name, value) for name, value in env.items())


def combine_commands(*commands: str) -> str:
    return " && ".join(commands)


def fuzz(
    fuzzer: Fuzzer,
    cpu_attr_arch_list: CpuAttrArchList,
    global_isel: bool,
    in_dir: str,
    out_dir: str,
    time: int,
    repeat: int,
    offset: int,
    type: ClutserType,
    jobs: int,
) -> None:
    matcher_table_size = (
        common.MATCHER_TABLE_SIZE_GISEL
        if global_isel
        else common.MATCHER_TABLE_SIZE_DAGISEL
    )
    isel = "gisel" if global_isel else "dagisel"

    tuples = []
    for r in range(repeat):
        for (cpu, attr, triple) in cpu_attr_arch_list:
            arch = common.TRIPLE_ARCH_MAP[triple]
            if arch not in matcher_table_size:
                logging.warn(
                    f"Can't find {triple}({arch})s' matcher table size, not fuzzing"
                )
                continue
            tuples.append((r + offset, cpu, attr, triple, arch))

    def process_creator(t) -> subprocess.Popen:
        r, cpu, attr, triple, arch = t
        logging.info(f"Fuzzing {cpu} {attr} {triple} {arch} -- {r}.")
        target = triple
        if cpu != "":
            target += "-" + cpu
        if attr != "":
            target += "-" + attr

        name = f"{fuzzer}-{isel}-{target}-{r}"
        expr_dir = f"{out_dir}/{fuzzer}/{isel}/{target}/{r}"

        fuzz_cmd = f"$FUZZING_HOME/$AFL/afl-fuzz -V {time} -i {in_dir} -o $OUTPUT"

        env = {}

        if fuzzer == "aflplusplus":
            env["AFL_CUSTOM_MUTATOR_ONLY"] = "0"
            env["AFL_CUSTOM_MUTATOR_LIBRARY"] = ""
        elif fuzzer == "libfuzzer":
            env["AFL_CUSTOM_MUTATOR_ONLY"] = "1"
            env[
                "AFL_CUSTOM_MUTATOR_LIBRARY"
            ] = "$FUZZING_HOME/mutator/build/libAFLFuzzMutate.so"
        elif fuzzer == "irfuzzer":
            env["AFL_CUSTOM_MUTATOR_ONLY"] = "1"
            env[
                "AFL_CUSTOM_MUTATOR_LIBRARY"
            ] = "$FUZZING_HOME/mutator/build/libAFLCustomIRMutator.so"
            fuzz_cmd += " -w"
        else:
            logging.fatal("UNREACHABLE")
            exit(1)

        fuzz_cmd += " $FUZZING_HOME/llvm-isel-afl/build/isel-fuzzing"

        env["CPU"] = cpu
        env["ATTR"] = attr
        env["TRIPLE"] = triple
        env["GLOBAL_ISEL"] = "1" if global_isel else "0"
        env["MATCHER_TABLE_SIZE"] = str(matcher_table_size[arch])

        command = ""

        if type == "screen":
            env["OUTPUT"] = expr_dir
            command = combine_commands(
                *get_export_commands(env),
                f"mkdir -p {expr_dir}",
                f'screen -S {name} -dm bash -c "{fuzz_cmd}"',
                f"sleep {time + 60}",
            )
        elif type == "docker":
            command_in_container = (
                combine_commands(
                    *get_export_commands(env),
                    fuzz_cmd,
                    f"chown -R {os.getuid()} /fuzzing/default",
                    "mv /fuzzing/default /output/default",
                )
                .replace("$", "\\$")
                .replace('"', '\\"')
            )

            command = combine_commands(
                f"mkdir -p {expr_dir}",
                f'docker run --cpus=1 --name={name} --rm --mount type=tmpfs,tmpfs-size=1G,dst=/fuzzing --env OUTPUT=/fuzzing -v {expr_dir}:/output {DOCKER_IMAGE} bash -c "{command_in_container}"',
            )
        elif type == "stdout":
            env["OUTPUT"] = expr_dir
            env["AFL_NO_UI"] = "1"
            command = combine_commands(
                *get_export_commands(env),
                f"mkdir -p {expr_dir}",
                fuzz_cmd,
            )
        else:
            logging.fatal("UNREACHABLE, type not set")

        process = subprocess.Popen(
            ["/bin/bash", "-c", command], stdout=subprocess.PIPE, stdin=subprocess.PIPE
        )
        # Sleep for 1s so aflplusplus has time to bind core. Otherwise two fuzzers may bind to the same core.
        subprocess.run(["sleep", "1"])
        return process

    common.parallel_subprocess(tuples, jobs, process_creator, None)


def main() -> None:
    args = Args().parse_args()

    output = os.path.abspath(args.output)
    if os.path.exists(output):
        logging.info(f"{args.output} already exists.")
        if args.on_exist == "force":
            logging.info(f"'on_exist' set to {args.on_exist}, will force remove")
            subprocess.run(["rm", "-rf", output])
        elif args.on_exist == "abort":
            logging.error(f"'on_exist' set to {args.on_exist}, won't work on it.")
            exit(1)

    fuzz(
        fuzzer=args.fuzzer,
        cpu_attr_arch_list=args.get_cpu_attr_arch_list(),
        global_isel=args.isel == "gisel",
        in_dir=args.input,
        out_dir=output,
        time=args.get_time_in_seconds(),
        repeat=args.repeat,
        offset=args.offset,
        type=args.type,
        jobs=args.jobs,
    )


if __name__ == "__main__":
    logging.basicConfig()
    logging.getLogger().setLevel(logging.INFO)
    main()
