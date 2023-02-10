import logging
from pathlib import Path
from typing import Dict, Iterable, List, Literal, NamedTuple, Optional, Tuple
import common
import subprocess
import os
import multiprocessing
from tap import Tap
import docker
from time import sleep

from collect_seeds import collect_seeds_from_tests


Fuzzer = Literal["aflplusplus", "libfuzzer", "irfuzzer"]
ISel = Literal["dagisel", "gisel"]
Tier = Literal[0, 1, 2, 3]
ClutserType = Literal["screen", "docker", "stdout"]
CpuAttrArchList = List[Tuple[str, str, str]]


DOCKER_IMAGE = "irfuzzer"
SECONDS_PER_UNIT: Dict[str, int] = {
    "s": 1,
    "m": 60,
    "h": 3600,
    "d": 86400,
    "w": 604800,
}
MUTATOR_LIBRARY_PATHS: Dict[Fuzzer, str] = {
    "aflplusplus": "",
    "libfuzzer": "mutator/build/libAFLFuzzMutate.so",
    "irfuzzer": "mutator/build/libAFLCustomIRMutator.so",
}


class ExperimentConfig(NamedTuple):
    fuzzer: Fuzzer
    triple: str
    cpu: str
    attr: str
    isel: ISel
    seed_dir: Path
    time: int
    replicate_id: int

    @property
    def target(self) -> str:
        target = self.triple
        if self.cpu != "":
            target += "-" + self.cpu
        if self.attr != "":
            target += "-" + self.attr
        return target

    @property
    def name(self) -> str:
        return f"{self.fuzzer}-{self.isel}-{self.target}-{self.replicate_id}"

    @property
    def matcher_table_size(self) -> Optional[int]:
        matcher_table_size = (
            common.MATCHER_TABLE_SIZE_GISEL
            if self.isel == "gisel"
            else common.MATCHER_TABLE_SIZE_DAGISEL
        )

        matcher_table = common.TRIPLE_ARCH_MAP[self.triple.split("-")[0]]

        if matcher_table not in matcher_table_size:
            return None

        return matcher_table_size[matcher_table]

    def get_fuzzing_env(self) -> Dict[str, str]:
        return {
            "AFL_CUSTOM_MUTATOR_ONLY": "0" if self.fuzzer == "aflplusplus" else "1",
            "AFL_CUSTOM_MUTATOR_LIBRARY": MUTATOR_LIBRARY_PATHS[self.fuzzer],
            "TRIPLE": self.triple,
            "CPU": self.cpu,
            "ATTR": self.attr,
            "GLOBAL_ISEL": "1" if self.isel == "gisel" else "0",
            "MATCHER_TABLE_SIZE": str(self.matcher_table_size),
        }

    def get_fuzzing_command(self, output_dir: str | Path) -> str:
        cmd = f"$AFL/afl-fuzz -V {self.time} -i {self.seed_dir} -o {output_dir}"

        if self.fuzzer == "irfuzzer":
            cmd += " -w"

        cmd += " llvm-isel-afl/build/isel-fuzzing"

        return cmd

    def get_output_dir(self, output_root_dir: Path) -> Path:
        return output_root_dir.joinpath(
            self.fuzzer,
            self.isel,
            self.target,
            str(self.replicate_id),
        )


class Args(Tap):
    """
    Command-line Arguments
    (Reference: https://github.com/swansonk14/typed-argument-parser)
    """

    fuzzer: Fuzzer = "irfuzzer"
    """the fuzzer used for fuzzing"""

    seeds: str
    """
    the directory containing input seeds for fuzzing (if 'seeding-from-tests' flag is not set)
    or the directory to store the seeds collected from tests (if 'seeding-from-tests' flag is set)
    """

    seeding_from_tests: bool = False
    """whether to use tests for each triple-cpu-attr combination as seeds for fuzzing"""

    output: str = "./fuzzing"
    """the output directory"""

    on_exist: Literal["abort", "force", "ignore"] = "abort"
    """the action to take if the output directory already exists"""

    isel: ISel = "dagisel"
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

    repeat: int = 1
    """how many times each experiemt should run"""

    offset: int = 0
    """the offset to start counting experiments"""

    jobs: int = multiprocessing.cpu_count()
    """the max number of concurrent subprocesses"""

    type: Optional[ClutserType] = None
    """the method to start fuzzing cluster"""

    def configure(self):
        self.add_argument("-j", "--jobs")
        self.add_argument("-o", "--output")
        self.add_argument("-r", "--repeat")
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


def get_experiment_configs(
    fuzzer: Fuzzer,
    cpu_attr_arch_list: CpuAttrArchList,
    seed_dir: Path,
    seeding_from_tests: bool,
    isel: ISel,
    time: int,
    repeat: int,
    offset: int,
) -> Iterable[ExperimentConfig]:
    for (cpu, attr, triple) in cpu_attr_arch_list:
        expr_seed_dir = seed_dir

        if seeding_from_tests:
            expr_seed_dir = collect_seeds_from_tests(
                out_dir_parent=seed_dir,
                arch_with_sub=triple.split("-")[0],
                triple=triple,
                cpu=None if cpu == "" else cpu,
                attrs=set() if attr == "" else set(attr.split(",")),
            )

        for r in range(repeat):
            expr_config = ExperimentConfig(
                fuzzer=fuzzer,
                triple=triple,
                cpu=cpu,
                attr=attr,
                isel=isel,
                seed_dir=expr_seed_dir,
                time=time,
                replicate_id=r + offset,
            )

            if expr_config.matcher_table_size is None:
                logging.warn(
                    f"Can't find matcher table size for triple '{triple}', not fuzzing"
                )
                continue

            yield expr_config


def combine_commands(*commands: str) -> str:
    return " && ".join(commands)


def batch_fuzz_using_docker(
    experiment_configs: List[ExperimentConfig],
    out_root: Path,
    jobs: int,
) -> None:
    """
    Run each experiment inside a dedicated Docker container.
    (Docker Python SDK Reference: https://docker-py.readthedocs.io/en/stable/)
    """

    client = docker.client.from_env()
    container_queue = []

    def dequeue_and_wait():
        dequeued_container = container_queue.pop(0)  # FIFO
        if dequeued_container.status != "exited":
            dequeued_container.wait()

    for i, experiment in enumerate(experiment_configs):
        if len(container_queue) == jobs:
            dequeue_and_wait()

        logging.info(f"Starting experiment {experiment.name}...")

        seed_dir = experiment.seed_dir
        out_dir = experiment.get_output_dir(out_root)
        out_dir.mkdir(parents=True)

        container = client.containers.run(
            image=DOCKER_IMAGE,
            command=[
                "bash",
                "-c",
                combine_commands(
                    # Docker is responsible for core binding,
                    # if AFL_NO_AFFINITY is not set, fuzzer will fail to start
                    "export AFL_NO_AFFINITY=1",
                    experiment.get_fuzzing_command("/fuzzing"),
                    f"chown -R {os.getuid()} /fuzzing/default",
                    "mv /fuzzing/default /output/default",
                ),
            ],
            remove=True,
            detach=True,
            name=experiment.name.replace("+", "").replace(",", "-"),
            environment=experiment.get_fuzzing_env(),
            cpuset_cpus=str(i % jobs),  # core binding
            tmpfs={"/fuzzing": "size=1G"},
            volumes=[
                f"{seed_dir.absolute()}:{seed_dir.absolute()}",
                f"{out_dir.absolute()}:/output",
            ],
        )

        container_queue.append(container)

    # wait for all running containers to exit
    while len(container_queue) > 0:
        dequeue_and_wait()


def batch_fuzz(
    experiment_configs: List[ExperimentConfig],
    out_root: Path,
    type: ClutserType,
    jobs: int,
) -> None:
    if type == "docker":
        batch_fuzz_using_docker(experiment_configs, out_root, jobs)
        return

    def start_subprocess(experiment: ExperimentConfig) -> subprocess.Popen:
        logging.info(f"Starting experiment {experiment.name}...")

        out_dir = experiment.get_output_dir(out_root)
        out_dir.mkdir(parents=True)

        env = experiment.get_fuzzing_env()

        if type == "stdout":
            env["AFL_NO_UI"] = "1"

        fuzzing_command = experiment.get_fuzzing_command(out_dir)

        if type == "screen":
            fuzzing_command = f'screen -S {experiment.name} -dm bash -c "{fuzzing_command}" && sleep {experiment.time + 30}'

        process = subprocess.Popen(
            fuzzing_command,
            env={**os.environ, **env},
            shell=True,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
        )

        # Sleep for 1s so aflplusplus has time to bind core. Otherwise two fuzzers may bind to the same core.
        sleep(1)

        return process

    common.parallel_subprocess(experiment_configs, jobs, start_subprocess, None)


def fuzz(expr_config: ExperimentConfig, out_root: Path) -> int:
    out_dir = expr_config.get_output_dir(out_root)
    out_dir.mkdir(parents=True)

    process = subprocess.run(
        expr_config.get_fuzzing_command(out_dir),
        env={**os.environ, **expr_config.get_fuzzing_env()},
        shell=True,
    )

    print(f"Fuzzing process exited with code {process.returncode}.")
    return process.returncode


def main() -> None:
    args = Args(underscores_to_dashes=True).parse_args()

    out_root = Path(args.output)
    if out_root.exists():
        logging.info(f"{args.output} already exists.")
        if args.on_exist == "force":
            logging.info(f"'on-exist' set to {args.on_exist}, will force remove")
            subprocess.run(["rm", "-rf", out_root])
        elif args.on_exist == "abort":
            logging.error(f"'on-exist' set to {args.on_exist}, won't work on it.")
            exit(1)

    expr_configs = list(
        get_experiment_configs(
            fuzzer=args.fuzzer,
            cpu_attr_arch_list=args.get_cpu_attr_arch_list(),
            seed_dir=Path(args.seeds),
            seeding_from_tests=args.seeding_from_tests,
            isel=args.isel,
            time=args.get_time_in_seconds(),
            repeat=args.repeat,
            offset=args.offset,
        )
    )

    if len(expr_configs) == 1 and args.type is None:
        exit(fuzz(expr_config=expr_configs[0], out_root=out_root))
    elif args.type is None:
        logging.error(
            "'--type' must be specified when running multiple fuzzing experiments"
        )
    else:
        batch_fuzz(
            experiment_configs=expr_configs,
            out_root=out_root,
            type=args.type,
            jobs=args.jobs,
        )


if __name__ == "__main__":
    logging.basicConfig()
    logging.getLogger().setLevel(logging.INFO)
    main()
