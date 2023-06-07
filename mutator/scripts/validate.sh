INPUT=$1
NPROC=$2

if [[ -z $INPUT ]]; then
    INPUT=$FUZZING_HOME/seeds/seed.bc
fi

if [[ -z $NPROC ]]; then
    export NPROC=`nproc --all`
fi

for J in $(seq $NPROC)
do
    rm O$J; touch O$J
    for I in $(seq 1000)
    do     
        SEED=$(shuf -i 0-4294967295 -n 1)
        $FUZZING_HOME/mutator/build_debug/MutatorDriver $INPUT $SEED -v
        if [[ $? -ne 0 ]]
        then
            echo $SEED &>> O$J
        fi
    done &
done
wait
# Try to match O* that is not empty.
ls -la | grep "$USER $USER [[:space:]]*[1-9].* [0-9]*:[0-9]* O[0-9]*"
