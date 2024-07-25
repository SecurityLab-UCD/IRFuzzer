from pathlib import Path
from typing import Iterable, NamedTuple, Optional
from tap import Tap
from tqdm import tqdm
from concurrent.futures import ProcessPoolExecutor

from batch_llc_cov import Coverage
from lib.experiment import get_all_experiments


class Args(Tap):
    """
    A wrapper of `batch_llc_cov.py` coupled to experiment results.

    Usage: `python scripts/batch_expr_cov.py -i <in_dir> -o <out_dir>`
    """

    in_dir: Path
    """
    Directory containing CSmith/GrayC/IRFuzzer experiement results.
    """

    out_dir: Path

    irfuzzer: bool = False
    """
    Whether the input directory follows IRFuzzer experiment result directory structure.
    """

    def configure(self) -> None:
        self.add_argument("-i", "--in-dir")
        self.add_argument("-o", "--out-dir")
        self.add_argument("--irfuzzer")


class BatchLLCCoverageJob(NamedTuple):
    in_dir: Path
    out_dir: Path
    replicate_id: str
    arch: str
    opt_level: int
    llc_flags: list[str]


def get_irfuzzer_cov_jobs(
    in_dir: Path,
    out_dir: Path,
) -> Iterable[BatchLLCCoverageJob]:
    for expr in get_all_experiments(in_dir):
        yield BatchLLCCoverageJob(
            in_dir=expr.queue_path,
            out_dir=out_dir.joinpath(expr.name),
            replicate_id=str(expr.replicate_id),
            arch=str(expr.target.triple),
            opt_level=3 if expr.fuzzer == "irfuzzer-attr-O3" else 2,
            llc_flags=[f"-mtriple={expr.target.triple}"],
        )


def get_non_irfuzzer_cov_jobs(
    in_dir: Path, out_dir: Path
) -> Iterable[BatchLLCCoverageJob]:
    for subdir in in_dir.iterdir():
        if not subdir.is_dir():
            continue

        for subsubdir in subdir.iterdir():
            if not subsubdir.is_dir():
                continue

            for subsubsubdir in subsubdir.iterdir():
                if not subsubsubdir.is_dir():
                    continue

                yield BatchLLCCoverageJob(
                    in_dir=subsubsubdir,
                    out_dir=out_dir.joinpath(subdir.name)
                    .joinpath(subsubdir.name)
                    .joinpath(subsubsubdir.name),
                    replicate_id=subdir.name,
                    arch=subsubdir.name,
                    opt_level=int(subsubsubdir.name),
                    llc_flags=[f"-mtriple={subsubdir.name}"],
                )


def execute(ir_collection: BatchLLCCoverageJob) -> Coverage | Exception:
    from batch_llc_cov import batch_llc_cov

    try:
        return batch_llc_cov(
            llc_flags=ir_collection.llc_flags,
            in_dir=ir_collection.in_dir,
            out_dir=ir_collection.out_dir,
            copy=True,
        )
    except Exception as e:
        return e


def main():
    args = Args(underscores_to_dashes=True).parse_args()

    ir_collections = list(
        get_irfuzzer_cov_jobs(args.in_dir, args.out_dir)
        if args.irfuzzer
        else get_non_irfuzzer_cov_jobs(args.in_dir, args.out_dir)
    )

    print(f"Found {len(ir_collections)} IR directories")

    args.out_dir.mkdir(parents=True)
    summary_csv = args.out_dir.joinpath("coverage-summary.csv").open("w+")
    error_txt = args.out_dir.joinpath("errors.txt").open("w+")

    print(
        "replicate_id,arch,opt_level,lines_hit,lines_total,branches_hit,branches_total,functions_hit,functions_total",
        file=summary_csv,
    )

    with ProcessPoolExecutor(max_workers=80) as executor:
        for ir_collection, coverage in tqdm(
            zip(ir_collections, executor.map(execute, ir_collections))
        ):
            if isinstance(coverage, Exception):
                print(
                    f"####### Error for {ir_collection} \n{coverage}\n########",
                    file=error_txt,
                )
                continue

            print(
                ",".join(
                    [
                        str(ir_collection.replicate_id),
                        ir_collection.arch,
                        str(ir_collection.opt_level),
                        str(coverage.lines[0]),
                        str(coverage.lines[1]),
                        str(coverage.branches[0]),
                        str(coverage.branches[1]),
                        str(coverage.functions[0]),
                        str(coverage.functions[1]),
                    ]
                ),
                file=summary_csv,
            )


if __name__ == "__main__":
    main()
