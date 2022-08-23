FUZZING_INSTANCES=3
if [ ! -z $1 ]
then
    FUZZING_INSTANCES=$1
fi
for I in $(seq 1 $FUZZING_INSTANCES)
do
    screen -S fuzzing-dagisel-$I -dm bash -c "export MATCHER_TABLE_SIZE=22600; $FUZZING_HOME/$AFL/afl-fuzz -i ~/$FUZZING_HOME/seeds/ -o ~/$FUZZING_HOME/fuzzing/fuzzing-dagisel-$I  -w /$FUZZING_HOME/llvm-isel-afl/build/isel-fuzzing; bash"
    screen -S fuzzing-globalisel-$I -dm bash -c "export GLOBAL_ISEL=1; export MATCHER_TABLE_SIZE=13780; $FUZZING_HOME/$AFL/afl-fuzz -i ~/$FUZZING_HOME/seeds/ -o ~/$FUZZING_HOME/fuzzing/fuzzing-globalisel-$I  -w /$FUZZING_HOME/llvm-isel-afl/build/isel-fuzzing; bash"
done
