import os
import subprocess
from typing import Iterable, Optional
from tap import Tap

from lib.process_concurrency import MAX_SUBPROCESSES, run_concurrent_subprocesses


class Args(Tap):
    input: str
    """The input directory containing C source code files"""

    output: str
    """The output directory for LLVM IR bytecode files"""

    opt_level: int
    """Optimization level for clang"""

    jobs: Optional[int] = None
    """The number of concurrent subprocesses"""

    csmith_root: str = "../csmith"
    """The root directory for CSmith repo"""

    def configure(self) -> None:
        self.add_argument("input")
        self.add_argument("-o", "--output")
        self.add_argument("-O", "--opt-level")
        self.add_argument("-j", "--jobs")


def build_clang_flags(
    target: str,
    sysroot: Optional[str] = None,
    include_paths: list[str] = [],
    opt_level: int = 0,
) -> Iterable[str]:
    yield f"--target={target}"
    yield f"-O{opt_level}"

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

    run_concurrent_subprocesses(
        iter=[
            file_name for file_name in os.listdir(src_dir) if file_name.endswith(".c")
        ],
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
        max_jobs=MAX_SUBPROCESSES if n_jobs is None else n_jobs,
    )


def main() -> None:
    args = Args(underscores_to_dashes=True).parse_args()

    def batch_compile_wrapper(
        target: str,
        sysroot: Optional[str] = None,
        include: Optional[str] = None,
        apt_package: Optional[str] = None,
        link: Optional[str] = None,
    ) -> None:
        if not os.path.exists(args.csmith_root):
            print(f"ERROR: missing CSmith in {args.csmith_root}.")
            print(
                f"Run `git clone https://github.com/csmith-project/csmith.git {args.csmith_root}`"
            )
            return

        if (include is not None and not os.path.exists(include)) or (
            sysroot is not None and not os.path.exists(sysroot)
        ):
            print(f"ERROR: missing headers for target {target}.")
            if apt_package is not None:
                print(f"Run `sudo apt install {apt_package}`.")
            if link is not None:
                print(f"See {link} for how to get the required headers.")
            return

        batch_compile(
            src_dir=args.input,
            out_dir=os.path.join(args.output, target),
            clang_flags=list(
                build_clang_flags(
                    target=target,
                    sysroot=sysroot,
                    include_paths=[os.path.join(args.csmith_root, "runtime")]
                    + ([] if include is None else [include]),
                    opt_level=args.opt_level,
                )
            ),
            n_jobs=args.jobs,
        )

    batch_compile_wrapper(
        "i686",
        include="/usr/i686-linux-gnu/include",
        apt_package="libc6-dev-i386-cross",
    )
    batch_compile_wrapper(
        "x86_64",
        include="/usr/x86_64-linux-gnu/include",
        apt_package="libc6-dev-amd64-cross",
    )
    batch_compile_wrapper(
        "arm",
        include="/usr/arm-linux-gnueabi/include",
        apt_package="libc6-dev-armel-cross",
    )
    batch_compile_wrapper(
        "aarch64",
        include="/usr/aarch64-linux-gnu/include",
        apt_package="libc6-dev-arm64-cross",
    )
    batch_compile_wrapper(
        "riscv64",
        include="/usr/riscv64-linux-gnu/include",
        apt_package="libc6-dev-riscv64-cross",
    )
    batch_compile_wrapper(
        "wasm32-wasi",
        sysroot="../wasi-sysroot",
        link="https://github.com/WebAssembly/wasi-sdk/releases/download/wasi-sdk-20/wasi-sysroot-20.0.tar.gz",
    )


if __name__ == "__main__":
    main()
