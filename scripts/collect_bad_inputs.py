from pathlib import Path
import random
import shutil
import subprocess
from typing import Iterable
from tap import Tap

from lib.experiment import get_all_experiments
from lib.time_parser import get_time_in_seconds


class Args(Tap):
    input: str
    """Path to the fuzzing output directory"""

    output: str
    """Path to the directory to write bad inputs and seeds to"""

    time: str
    """
    the threshold duration for an experiment to be considered failed.
    Current input that cause an experiment to fail in less than this time
    is considered a bad input.
    (e.g. '100s', '30m', '2h', '1d')
    """

    n: int = 256
    """Number of random seeds to test for each bad input"""

    driver: str = "mutator/build/MutatorDriver"
    """Path to the mutator driver executable"""

    def configure(self) -> None:
        self.add_argument("input")
        self.add_argument("-o", "--output")
        self.add_argument("-t", "--time")

    def get_time_in_seconds(self) -> int:
        return get_time_in_seconds(self.time)


def copy_bad_inputs(
    fuzzing_out_dir: Path, out_dir: Path, time_secs: int
) -> Iterable[Path]:
    for expr in get_all_experiments(fuzzing_out_dir):
        if expr.run_time < time_secs:
            dest_path = out_dir.joinpath(expr.name + ".bc")
            shutil.copy(expr.cur_input_path, dest_path)
            yield dest_path


def mutate(mutator_driver: Path, input_bc: Path, seed: int) -> int:
    return subprocess.run(
        [mutator_driver, input_bc, str(seed)],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    ).returncode


def collect_bad_seeds(mutator_driver: Path, input_bc: Path, n: int) -> Iterable[int]:
    for _ in range(n):
        seed = random.randint(0, 4294967295)
        ret_code = mutate(mutator_driver, input_bc, seed)
        if ret_code != 0:
            yield seed


def main() -> None:
    args = Args().parse_args()

    out_dir = Path(args.output)
    out_dir.mkdir(exist_ok=True)

    bad_input_paths = list(
        copy_bad_inputs(
            fuzzing_out_dir=Path(args.input),
            out_dir=out_dir,
            time_secs=args.get_time_in_seconds(),
        )
    )

    print(f"{len(bad_input_paths)} bad inputs written to dir {out_dir}.")

    for bad_input_path in bad_input_paths:
        print(f"Collecting bad seeds for {bad_input_path}...")

        bad_input_path.parent.joinpath(
            bad_input_path.name.removesuffix(".bc") + ".seeds.txt"
        ).write_text(
            "\n".join(
                str(seed)
                for seed in collect_bad_seeds(
                    mutator_driver=Path(args.driver),
                    input_bc=bad_input_path,
                    n=args.n,
                )
            )
        )

    print("Done.")


if __name__ == "__main__":
    main()
