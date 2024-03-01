INPUT=$1
NPROC=$2
JOB_PER_CORE=$3

if [[ -z $INPUT ]]; then
    INPUT=$FUZZING_HOME/seeds/seed.bc
fi

if [[ -z $NPROC ]]; then
    export NPROC=`nproc --all`
fi

if [[ -z $JOB_PER_CORE ]]; then
    export JOB_PER_CORE=1000
fi

for J in $(seq $NPROC)
do
    rm O$J; touch O$J
    for SEED in $(shuf -i 0-4294967295 -n $JOB_PER_CORE)
    do     
        $FUZZING_HOME/mutator/build-debug/MutatorDriver $INPUT $SEED -v
        if [[ $? -ne 0 ]]
        then
            echo $SEED &>> O$J
        fi
    done &
done
wait
# Try to match O* that is not empty.
ls -la | grep "$USER $USER [[:space:]]*[1-9].* [0-9]*:[0-9]* O[0-9]*"
