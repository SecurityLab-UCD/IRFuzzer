from pathlib import Path
from typing import Iterable, NamedTuple, Optional
from bitarray import bitarray

import pandas as pd
from lib.fs import subdirs_of
from lib.plot_data import read_plot_data

from lib.target import Target
from lib.matcher_table_sizes import (
    DAGISEL_MATCHER_TABLE_SIZES,
    GISEL_MATCHER_TABLE_SIZES,
)
from lib.coverage_map import count_non_255_bytes, read_coverage_map


BITMAP_SIZE_IN_BYTES = 65536


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
    def matcher_table_size(self) -> int:
        if self.isel == "dagisel":
            return DAGISEL_MATCHER_TABLE_SIZES[self.target.backend]
        elif self.isel == "gisel":
            return GISEL_MATCHER_TABLE_SIZES[self.target.backend]
        else:
            raise Exception("Invalid ISel")

    @property
    def plot_data_path(self) -> Path:
        return self.path / "default" / "plot_data"

    @property
    def fuzzer_stats_path(self) -> Path:
        return self.path / "default" / "fuzzer_stats"

    @property
    def cur_input_path(self) -> Path:
        return self.path / "default" / ".cur_input"

    @property
    def initial_bitmap_path(self) -> Path:
        return self.path / "default" / "fuzz_initial_bitmap"

    @property
    def bitmap_path(self) -> Path:
        return self.path / "default" / "fuzz_bitmap"

    @property
    def initial_shadow_map_path(self) -> Path:
        return self.path / "default" / "fuzz_initial_shadowmap"

    @property
    def shadow_map_path(self) -> Path:
        return self.path / "default" / "fuzz_shadowmap"

    @property
    def intial_bitmap(self) -> bitarray:
        return read_coverage_map(self.initial_bitmap_path, BITMAP_SIZE_IN_BYTES * 8)

    @property
    def bitmap(self) -> bitarray:
        return read_coverage_map(self.bitmap_path, BITMAP_SIZE_IN_BYTES * 8)

    @property
    def intial_shadow_map(self) -> bitarray:
        return read_coverage_map(self.initial_shadow_map_path, self.matcher_table_size)

    @property
    def shadow_map(self) -> bitarray:
        return read_coverage_map(self.shadow_map_path, self.matcher_table_size)

    @property
    def initial_branch_coverage(self) -> float | None:
        try:
            return count_non_255_bytes(self.initial_bitmap_path) / BITMAP_SIZE_IN_BYTES
        except FileNotFoundError:
            return None

    @property
    def branch_coverage(self) -> float | None:
        try:
            return count_non_255_bytes(self.bitmap_path) / BITMAP_SIZE_IN_BYTES
        except FileNotFoundError:
            return None

    @property
    def initial_matcher_table_coverage(self) -> float | None:
        try:
            return self.intial_shadow_map.count(0) / self.matcher_table_size
        except FileNotFoundError:
            return None

    @property
    def matcher_table_coverage(self) -> float | None:
        try:
            return self.shadow_map.count(0) / self.matcher_table_size
        except FileNotFoundError:
            return None

    @property
    def run_time(self) -> int:
        s = self["run_time"]
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
