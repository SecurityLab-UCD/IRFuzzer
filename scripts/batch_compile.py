import argparse
import multiprocessing
import os
import subprocess
from typing import Iterable, Optional

from common import parallel_subprocess


def build_clang_flags(
    target: str,
    sysroot: Optional[str] = None,
    include_paths: list[str] = [],
    opt_level: str = "0",
) -> Iterable[str]:
    yield f"--target={target}"
    yield "-O" + opt_level

    if sysroot is not None:
        yield f"--sysroot={sysroot}"

    for include_path in include_paths:
        yield f"-I{include_path}"

    yield "-emit-llvm"
    yield "-c"


def batch_compile(
    src_dir: str, out_dir: str, clang_flags: list[str], n_jobs: Optional[int] = None
) -> None:
    print(
        f'Compiling source code in {src_dir} to {out_dir} using "clang {" ".join(clang_flags)}"...'
    )

    os.makedirs(out_dir, exist_ok=True)

    parallel_subprocess(
        iter=[
            file_name for file_name in os.listdir(src_dir) if file_name.endswith(".c")
        ],
        jobs=(multiprocessing.cpu_count() - 1) if n_jobs is None else n_jobs,
        subprocess_creator=lambda file_name: subprocess.Popen(
            args=[
                "clang",
                *clang_flags,
                os.path.join(src_dir, file_name),
                "-o",
                os.path.join(out_dir, file_name.replace(".c", ".bc")),
            ],
            stderr=subprocess.DEVNULL,
            stdout=subprocess.DEVNULL,
        ),
    )


def main() -> None:
    parser = argparse.ArgumentParser(description="Batch compiling C code to LLVM IR")

    parser.add_argument(
        "-i",
        "--input",
        type=str,
        required=True,
        help="The input directory containing C source code files",
    )

    parser.add_argument(
        "-o",
        "--output",
        type=str,
        required=True,
        help="The output directory for LLVM IR bytecode files",
    )

    parser.add_argument(
        "-j",
        "--jobs",
        type=int,
        help="The number of concurrent subprocesses",
    )

    args = parser.parse_args()

    def batch_compile_wrapper(
        target: str, sysroot: Optional[str] = None, include: Optional[str] = None
    ) -> None:
        batch_compile(
            src_dir=args.input,
            out_dir=os.path.join(args.output, target),
            clang_flags=list(
                build_clang_flags(
                    target=target,
                    sysroot=sysroot,
                    include_paths=["../csmith/runtime/"]
                    + ([] if include is None else [include]),
                    opt_level="2",
                )
            ),
            n_jobs=args.jobs,
        )

    batch_compile_wrapper("i686", include="/usr/i686-linux-gnu/include")
    batch_compile_wrapper("x86_64", include="/usr/x86_64-linux-gnu/include")
    batch_compile_wrapper("arm", include="/usr/arm-linux-gnueabi/include")
    batch_compile_wrapper("aarch64", include="/usr/aarch64-linux-gnu/include")
    batch_compile_wrapper("riscv32", sysroot="./riscv32/sysroot")
    batch_compile_wrapper("riscv64", include="/usr/riscv64-linux-gnu/include")
    batch_compile_wrapper("wasm32-wasi", sysroot="./wasi-sdk-14.0/share/wasi-sysroot")


if __name__ == "__main__":
    main()