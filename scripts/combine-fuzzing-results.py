import os

from common import subdirs_of


def merge_subdirs_by_symlink(src: str, dest: str) -> None:
    for machine_dir in subdirs_of(src):
        for fuzzer_dir in subdirs_of(machine_dir.path):
            for isel_dir in subdirs_of(fuzzer_dir.path):
                for arch_dir in subdirs_of(isel_dir.path):
                    symlink_dest_dir = os.path.join(
                        dest, fuzzer_dir.name, isel_dir.name, arch_dir.name
                    )

                    os.makedirs(symlink_dest_dir, exist_ok=True)

                    i = max([
                        -1,
                        *(
                            int(dir_entry.name)
                            for dir_entry in subdirs_of(symlink_dest_dir)
                        )
                    ])

                    for inner_dir in sorted(subdirs_of(arch_dir.path), key=lambda dir: int(dir.name)):
                        i += 1
                        symlink_src = os.path.join(
                            '../../../..', inner_dir.path
                        )
                        symlink_dest = os.path.join(
                            symlink_dest_dir, str(i)
                        )
                        print(
                            symlink_dest, " -> ", symlink_src,
                            sep="\t", end="\t", flush=True
                        )
                        os.symlink(symlink_src, symlink_dest)
                        print("DONE", flush=True)


def main() -> None:
    # make sure current working directory is archive before running this
    merge_subdirs_by_symlink('.', './combined')


if __name__ == "__main__":
    main()
