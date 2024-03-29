from pathlib import Path
from typing import Iterable, NamedTuple, Optional

import pandas as pd
from lib.fs import subdirs_of
from lib.plot_data import read_plot_data

from lib.target import Target


class Experiment(NamedTuple):
    path: Path
    fuzzer: str
    isel: str
    target: Target
    replicate_id: int

    @property
    def name(self) -> str:
        return f"{self.fuzzer}:{self.isel}:{self.target}:{self.replicate_id}"

    @property
    def plot_data_path(self) -> Path:
        return self.path.joinpath("default", "plot_data")

    @property
    def fuzzer_stats_path(self) -> Path:
        return self.path.joinpath("default", "fuzzer_stats")
    
    @property
    def cur_input_path(self) -> Path:
        return self.path.joinpath("default", ".cur_input")
    
    @property
    def run_time(self) -> int:
        s = self['run_time']
        return -1 if s is None else int(s)

    def __getitem__(self, key: str) -> Optional[str]:
        if not self.fuzzer_stats_path.exists():
            return None

        with open(self.fuzzer_stats_path) as f:
            for line in f:
                if line.startswith(key):
                    return line.split(" : ")[1]
        
        return None

    def read_plot_data(self) -> pd.DataFrame:
        return read_plot_data(self.plot_data_path)


def get_all_experiments(root_dir: Path | str) -> Iterable[Experiment]:
    for fuzzer_dir in subdirs_of(root_dir):
        for isel_dir in subdirs_of(fuzzer_dir.path):
            for target_dir in sorted(
                subdirs_of(isel_dir.path), key=lambda dir: dir.name
            ):
                for replicate_dir in subdirs_of(target_dir.path):
                    yield Experiment(
                        path=Path(replicate_dir.path),
                        fuzzer=fuzzer_dir.name.split(".")[0],
                        isel=isel_dir.name,
                        target=Target.parse(target_dir.name),
                        replicate_id=int(replicate_dir.name),
                    )
