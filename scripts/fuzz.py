import logging
import common
import subprocess
import argparse


def fuzz(argv):
    if argv.isel == "gisel":
        GLOBAL_ISEL = 1
        MATCHER_TABLE_SIZE = common.MATCHER_TABLE_SIZE_GISEL
    elif argv.isel == "dagisel":
        GLOBAL_ISEL = 0
        MATCHER_TABLE_SIZE = common.MATCHER_TABLE_SIZE_DAGISEL
    else:
        logging.fatal("UNREACHABLE, isel not set.")

    tuples = []
    for r in range(argv.repeat):
        for triple, arch in common.TRIPLE_ARCH_MAP.items():
            if arch not in MATCHER_TABLE_SIZE:
                logging.info(
                    f"Can't find {triple}({arch})s' matcher table size, not fuzzing ")
                continue
            tuples.append((r, triple, arch))

    def process_creator(t):
        r, triple, arch = t
        logging.info(
            f"Fuzzing {triple}({arch}) -- {r}.")
        isel = "dagisel" if GLOBAL_ISEL == 0 else "gisel"
        cmd = ['screen', '-S', f'{triple}-{isel}-{r}', '-dm', 'bash', '-c',
               f'export MATCHER_TABLE_SIZE={MATCHER_TABLE_SIZE[arch]}; echo $MATCHER_TABLE_SIZE; sleep 10;"']
        return subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stdin=subprocess.PIPE)


def main() -> None:
    parser = argparse.ArgumentParser(description='Run all fuzzers')
    parser.add_argument('-i', '--input', type=str, default="./seeds/",
                        help='The directory containing input seeds, default to ./seeds')
    parser.add_argument('-o', '--output', type=str, default="./fuzzing",
                        help="The directory to store all organized ./fuzzing/")
    parser.add_argument('--force', action='store_true',
                        help="force delete the output directory if it already exists.")
    parser.add_argument(
        '-f', '--fuzzer', choices=['aflplusplus', 'libfuzzer', 'aflisel'], help="The fuzzer we are using for fuzzing.")
    parser.add_argument('-j', '--jobs', type=int, default=40,
                        help="Max number of jobs parallel, default 40.")
    parser.add_argument('-r', '--repeat', type=int, default=5,
                        help="Numbers to repeat one experiment.")
    parser.add_argument('--isel', choices=["gisel", "dagisel"],
                        required=True, help="The isel alorighm you want to run.")
    args = parser.parse_args()

    fuzz(args)


if __name__ == "__main__":
    logging.basicConfig()
    logging.getLogger().setLevel(logging.INFO)
    main()
