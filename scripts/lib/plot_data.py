from pathlib import Path
import pandas as pd


def __convert_percentage_to_float(s: str) -> float:
    return float(s.strip("%")) / 100


def read_plot_data(file_path: Path) -> pd.DataFrame:
    # the table header is not consistent, so we don't want pandas to detect and process the header (1st row of csv)
    # but let it use the hard-coded column names below.
    return pd.read_csv(
        file_path,
        index_col=False,
        header=None,
        skiprows=1,
        names=[
            "# relative_time",
            "cycles_done",
            "cur_item",
            "corpus_count",
            "pending_total",
            "pending_favs",
            "bit_cvg",
            "shw_cvg",
            "saved_crashes",
            "saved_hangs",
            "max_depth",
            "execs_per_sec",
            "total_execs",
            "edges_found",
        ],
        converters={
            "bit_cvg": __convert_percentage_to_float,
            "shw_cvg": __convert_percentage_to_float,
        },
    )
