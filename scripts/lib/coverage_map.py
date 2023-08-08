from math import ceil
from pathlib import Path
from bitarray import bitarray


def read_coverage_map(path: Path, matcher_table_size: int) -> bitarray:
    cvg_map = bitarray()

    with open(path, "rb") as file:
        cvg_map.fromfile(file)
        assert ceil(matcher_table_size / 8) * 8 == len(cvg_map)
        cvg_map = cvg_map[:matcher_table_size]

    return cvg_map
