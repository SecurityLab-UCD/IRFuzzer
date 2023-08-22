from pathlib import Path
from bitarray import bitarray


def read_coverage_map(path: Path, real_bitmap_size_in_bits: int) -> bitarray:
    cvg_map = bitarray()

    with open(path, "rb") as file:
        cvg_map.fromfile(file)
        assert real_bitmap_size_in_bits <= len(cvg_map)
        cvg_map = cvg_map[:real_bitmap_size_in_bits]

    return cvg_map


def count_non_255_bytes(path: Path, chunk_size: int = 4096) -> int:
    count = 0

    with open(path, 'rb') as file:
        while True:
            chunk = file.read(chunk_size)  # Read data in chunks
            if not chunk:
                break  # Reached the end of the file
            count += chunk_size - chunk.count(b'\xff')

    return count
