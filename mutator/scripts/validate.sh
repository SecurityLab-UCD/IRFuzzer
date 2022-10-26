export NPROC=`nproc --all`
# export NPROC=1
for J in $(seq $NPROC)
do
    for I in $(seq 10000)
    do     
        $FUZZING_HOME/mutator/build/MutatorDriver $FUZZING_HOME/seeds/seed.bc $(shuf -i 0-4294967295 -n 1) -v &>> O$J
    done &
done
wait
# Try to match O* that is not empty.
ls -la | grep "$USER $USER [[:space:]]*[1-9].* [0-9]*:[0-9]* O[0-9]*"