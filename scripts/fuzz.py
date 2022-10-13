import logging
import common
import subprocess
import argparse
import os
from typing import List


def fuzz(argv):
    if os.path.exists(argv.output):
        if argv.force:
            logging.info(f"{argv.output} already exists, will force remove")
            subprocess.run(["rm", "-rf", argv.output])
        else:
            logging.info(f"{argv.output} already exists, won't work on it.")
            return

    if argv.isel == "gisel":
        global_isel = 1
        matcher_table_size = common.MATCHER_TABLE_SIZE_GISEL
    elif argv.isel == "dagisel":
        global_isel = 0
        matcher_table_size = common.MATCHER_TABLE_SIZE_DAGISEL
    else:
        logging.fatal("UNREACHABLE, isel not set.")

    triple_arch_map = {}
    if argv.tier == 0:
        triple_arch_map = common.TRIPLE_ARCH_MAP
    elif argv.tier == 1:
        triple_arch_map = common.TRIPLE_ARCH_MAP_TIER_1
    elif argv.tier == 2:
        triple_arch_map = common.TRIPLE_ARCH_MAP_TIER_2
    else:
        logging.fatal("UNREACHABLE")
    if argv.set is not None:
        triple_arch_map = {}
        for triple in argv.set:
            triple_arch_map[triple] = common.TRIPLE_ARCH_MAP[triple]

    isel = "dagisel" if global_isel == 0 else "gisel"
    tuples = []
    for r in range(argv.repeat):
        for triple, arch in triple_arch_map.items():
            if arch not in matcher_table_size:
                logging.info(
                    f"Can't find {triple}({arch})s' matcher table size, not fuzzing ")
                continue
            tuples.append((r, triple, arch))

    def process_creator(t):
        r, triple, arch = t
        logging.info(
            f"Fuzzing {triple}({arch}) -- {r}.")
        name = f"{r}"
        verbose_name = f"{argv.fuzzer}-{isel}-{triple}-{r}"
        proj_dir = f"{argv.output}/{argv.fuzzer}/{isel}/{triple}/{name}"

        fuzz_cmd = f"$FUZZING_HOME/$AFL/afl-fuzz -i $FUZZING_HOME/seeds/ -o {proj_dir} $FUZZING_HOME/llvm-isel-afl/build/isel-fuzzing"

        if argv.fuzzer == "aflplusplus":
            fuzzer_specific = f'''
            export AFL_CUSTOM_MUTATOR_ONLY=0
            export AFL_CUSTOM_MUTATOR_LIBRARY="";
            '''
        elif argv.fuzzer == "libfuzzer":
            fuzzer_specific = f'''
            export AFL_CUSTOM_MUTATOR_ONLY=1
            export AFL_CUSTOM_MUTATOR_LIBRARY=$FUZZING_HOME/mutator/build/libAFLFuzzMutate.so;
            '''
        elif argv.fuzzer == "aflisel":
            fuzzer_specific = f'''
            export AFL_CUSTOM_MUTATOR_ONLY=1
            export AFL_CUSTOM_MUTATOR_LIBRARY=$FUZZING_HOME/mutator/build/libAFLCustomIRMutator.so;
            '''
            fuzz_cmd = fuzz_cmd.split(" ")
            fuzz_cmd.insert(-1, "-w")
            fuzz_cmd = " ".join(fuzz_cmd)
        else:
            logging.warn("UNREACHABLE")
        env_exporting = f'''
            {fuzzer_specific}
            export TRIPLE={triple};
            export global_isel={global_isel};
            export MATCHER_TABLE_SIZE={matcher_table_size[arch]};
        '''

        config_file = f"{proj_dir}/config"
        config_logging = f'''
            mkdir -p {proj_dir}/{name}
            echo "Fuzzing instruction selection." > {config_file}
            echo "FUZZER: {argv.fuzzer}" >> {config_file}
            echo "FORCE_REMOVAL: {argv.force}" >> {config_file}
            echo "MATCHER_TABLE_SIZE: $MATCHER_TABLE_SIZE" >> {config_file}
            echo "AFL_CUSTOM_MUTATOR_ONLY: $AFL_CUSTOM_MUTATOR_ONLY" >> {config_file}
            echo "AFL_CUSTOM_MUTATOR_LIBRARY: $AFL_CUSTOM_MUTATOR_LIBRARY" >> {config_file}
            echo "TIMEOUT: {argv.time}" >> {config_file}
            echo "CMD: {fuzz_cmd}" >> {config_file}
            echo "TIMEOUT: {argv.time}" >> {config_file}
        '''
        command = f'''
            {env_exporting}

            {config_logging}

            echo "START_TIME: $(date)" >> {config_file}
            screen -S {verbose_name} -dm bash -c "timeout {argv.time} {fuzz_cmd}"
            echo "END_TIME: $(date)" >> {config_file}

            sleep {argv.time}
            exit
        '''.encode()
        process = subprocess.Popen(
            ['/bin/bash', '-c', command], stdout=subprocess.PIPE, stdin=subprocess.PIPE)
        # Sleep for 100ms so aflplusplus has time to bind core. Otherwise two fuzzers may bind to the same core.
        subprocess.run(["sleep", "0.1"])
        return process

    common.parallel_subprocess(tuples, argv.jobs, process_creator, None)


def main() -> None:
    parser = argparse.ArgumentParser(description='Run all fuzzers')
    parser.add_argument('-i', '--input', type=str, default="./seeds/",
                        help='The directory containing input seeds, default to ./seeds')
    parser.add_argument('-o', '--output', type=str, default="./fuzzing",
                        help="The directory to store all organized ./fuzzing/")
    parser.add_argument('--force', action='store_true',
                        help="force delete the output directory if it already exists.")
    parser.add_argument('--fuzzer', choices=['aflplusplus', 'libfuzzer', 'aflisel'],
                        default='aflplusplus', help="The fuzzer we are using for fuzzing.")
    parser.add_argument('-j', '--jobs', type=int, default=80,
                        help="Max number of jobs parallel, default 40.")
    parser.add_argument('-r', '--repeat', type=int, default=3,
                        help="Numbers to repeat one experiment.")
    parser.add_argument('--isel', choices=["gisel", "dagisel"],
                        required=True, help="The isel alorighm you want to run.")
    parser.add_argument('-t', '--time', type=str,
                        default='5m', help="Total time to run fuzzers")
    parser.add_argument('--tier', type=int, choices=[0, 1, 2],
                        help="The set of triples to test. 0 corresponds to everything, 1 and 2 corresponds to Tier 1 and Tier 2, see common.py for more. Will be overriden by `--set`")
    parser.add_argument(
        '--set', type=List[str], help="Select the triples to run.")
    args = parser.parse_args()

    def convert_to_seconds(s: str) -> int:
        seconds_per_unit = {"s": 1, "m": 60,
                            "h": 3600, "d": 86400, "w": 604800}
        return int(s[:-1]) * seconds_per_unit[s[-1]]
    args.time = convert_to_seconds(args.time)

    fuzz(args)


if __name__ == "__main__":
    logging.basicConfig()
    logging.getLogger().setLevel(logging.INFO)
    main()
