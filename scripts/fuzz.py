import logging
from pathlib import Path
import subprocess
from typing import Iterable, Literal, NamedTuple, Optional
import os
from tap import Tap
import docker
from time import sleep

from collect_seeds import TargetProp, collect_seeds_from_tests
from lib.arch import ARCH_TO_BACKEND_MAP
from lib.process_concurrency import MAX_SUBPROCESSES, run_concurrent_subprocesses
from lib.target import Target
from lib.matcher_table_sizes import (
    DAGISEL_MATCHER_TABLE_SIZES,
    GISEL_MATCHER_TABLE_SIZES,
)
from lib.target_lists import TARGET_LIST_TIER_1, TARGET_LIST_TIER_2, TARGET_LIST_TIER_3


Fuzzer = Literal["aflplusplus", "libfuzzer", "irfuzzer"]
ISel = Literal["dagisel", "gisel"]
Tier = Literal[0, 1, 2, 3]
ClutserType = Literal["screen", "docker", "stdout"]


DOCKER_IMAGE = "irfuzzer"
SECONDS_PER_UNIT: dict[str, int] = {
    "s": 1,
    "m": 60,
    "h": 3600,
    "d": 86400,
    "w": 604800,
}
MUTATOR_LIBRARY_PATHS: dict[Fuzzer, str] = {
    "aflplusplus": "",
    "libfuzzer": "mutator/build/libAFLFuzzMutate.so",
    "irfuzzer": "mutator/build/libAFLCustomIRMutator.so",
}


class ExperimentConfig(NamedTuple):
    fuzzer: Fuzzer
    target: Target
    isel: ISel
    seed_dir: Path
    time: int
    replicate_id: int

    @property
    def name(self) -> str:
        return f"{self.fuzzer}:{self.isel}:{self.target}:{self.replicate_id}"

    @property
    def matcher_table_size(self) -> Optional[int]:
        matcher_table_sizes = (
            GISEL_MATCHER_TABLE_SIZES
            if self.isel == "gisel"
            else DAGISEL_MATCHER_TABLE_SIZES
        )

        backend = self.target.backend

        if backend not in matcher_table_sizes:
            return None

        return matcher_table_sizes[backend]

    def get_fuzzing_env(self) -> dict[str, str]:
        return {
            "AFL_CUSTOM_MUTATOR_ONLY": "0" if self.fuzzer == "aflplusplus" else "1",
            "AFL_CUSTOM_MUTATOR_LIBRARY": MUTATOR_LIBRARY_PATHS[self.fuzzer],
            "TRIPLE": str(self.target.triple),
            "CPU": self.target.cpu if self.target.cpu else "",
            "ATTR": ",".join(self.target.attrs),
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
            str(self.target),
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
    """whether to use tests as seeds for fuzzing"""

    props_to_match: list[TargetProp] = ["triple", "cpu", "attrs"]
    """
    the properties of a test target to match those of the fuzzing target,
    used to determine which tests should be included as seeds.
    (if 'seeding_from_tests' flag is not set, this option as no effect)
    """

    timeout: Optional[float] = 0.1
    """
    only include test cases that can be compiled within the specified in seconds.
    (if 'seeding_from_tests' flag is not set, this option as no effect)
    """

    output: str = "./fuzzing"
    """the output directory"""

    on_exist: Literal["abort", "force", "ignore"] = "abort"
    """the action to take if the output directory already exists"""

    isel: ISel = "dagisel"
    """the LLVM instruction selection method to fuzz"""

    tier: Optional[Tier] = None
    """
    the set of targets to fuzz
    (0: everything, 1: Tier 1, 2: Tier 2, see 'lib/target_lists.py' for details)
    (can be overriden by `--targets`)
    """

    targets: Optional[list[str]] = None
    """
    manually specify targets to fuzz ('tier' will be ignored).
    Format for each target can be
    "<triple> [<cpu>] [<attr1> <attr2> ...]",
    "<triple> [<cpu>] [<attr1>,<attr2>,...]", or
    "<triple>[,<cpu>][,<attr1>,<attr2>,...]".
    (An attribute must start with '+' or '-' to avoid ambiguity.)
    """

    time: str = "5m"
    """duration for each experiment (e.g. '100s', '30m', '2h', '1d')"""

    repeat: int = 1
    """how many times each experiemt should run"""

    offset: int = 0
    """the offset to start counting experiments"""

    jobs: int = MAX_SUBPROCESSES
    """the max number of concurrent subprocesses"""

    type: Optional[ClutserType] = None
    """the method to start fuzzing cluster"""

    def configure(self):
        self.add_argument("-j", "--jobs")
        self.add_argument("-o", "--output")
        self.add_argument("-r", "--repeat")
        self.add_argument("-t", "--time")

    def get_fuzzing_targets(self) -> list[Target]:
        if self.tier == 0:
            return [Target(triple=arch) for arch in ARCH_TO_BACKEND_MAP.keys()]
        elif self.tier == 1:
            return TARGET_LIST_TIER_1
        elif self.tier == 2:
            return TARGET_LIST_TIER_2
        elif self.tier == 3:
            return TARGET_LIST_TIER_3
        elif self.targets is not None:
            return [Target.parse(s) for s in self.targets]
        else:
            logging.error("Either '--tier' or '--set' has to be specified.")
            exit(1)

    def get_time_in_seconds(self) -> int:
        return int(self.time[:-1]) * SECONDS_PER_UNIT[self.time[-1]]


def get_experiment_configs(
    fuzzer: Fuzzer,
    isel: ISel,
    targets: list[Target],
    time: int,
    repeat: int,
    offset: int,
    seed_dir: Path,
    seeding_from_tests: bool,
    props_to_match: list[TargetProp],
    compilation_timout_secs: Optional[float],
) -> Iterable[ExperimentConfig]:
    for target in targets:
        expr_seed_dir = seed_dir

        if seeding_from_tests:
            expr_seed_dir = collect_seeds_from_tests(
                target=target,
                global_isel=isel == "gisel",
                out_dir_parent=seed_dir,
                props_to_match=props_to_match,
                dump_bc=True,
                symlink_to_ll=False,
                timeout_secs=compilation_timout_secs,
            )

        for r in range(repeat):
            expr_config = ExperimentConfig(
                fuzzer=fuzzer,
                target=target,
                isel=isel,
                seed_dir=expr_seed_dir,
                time=time,
                replicate_id=r + offset,
            )

            if expr_config.matcher_table_size is None:
                logging.warn(
                    f"Can't find matcher table size for target '{expr_config.target}', not fuzzing"
                )
                continue

            yield expr_config


def combine_commands(*commands: str) -> str:
    return " && ".join(commands)


def batch_fuzz_using_docker(
    experiment_configs: list[ExperimentConfig],
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
            name=experiment.name.replace("+", "").replace(",", "-").replace(":", "-"),
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
    experiment_configs: list[ExperimentConfig],
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
            # If using screen, this script will not be able to detect whether the fuzzing process fails early or did not
            # complete within the estimated time.
            fuzzing_command = f'screen -S "{experiment.name}" -dm bash -c "{fuzzing_command}" && sleep {experiment.time + 180}'

        process = subprocess.Popen(
            fuzzing_command,
            env={**os.environ, **env},
            shell=True,
            stdin=subprocess.PIPE,
            stdout=subprocess.DEVNULL,
        )

        # Sleep for 1s so aflplusplus has time to bind core. Otherwise two fuzzers may bind to the same core.
        sleep(1)

        return process

    run_concurrent_subprocesses(
        iter=experiment_configs,
        subprocess_creator=start_subprocess,
        on_exit=lambda expr_cfg, exit_code, p: print(
            f"Experiment {expr_cfg.name} exited with code {exit_code}"
        ),
        max_jobs=jobs,
    )


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
            isel=args.isel,
            targets=args.get_fuzzing_targets(),
            time=args.get_time_in_seconds(),
            repeat=args.repeat,
            offset=args.offset,
            seed_dir=Path(args.seeds),
            seeding_from_tests=args.seeding_from_tests,
            props_to_match=args.props_to_match,
            compilation_timout_secs=args.timeout,
        )
    )

    print(
        f"\nThe following {len(expr_configs)} experiment(s) will start in 10 seconds:\n"
    )
    for expr in expr_configs:
        print(f" - {expr.name}")
    print()

    sleep(10)

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
