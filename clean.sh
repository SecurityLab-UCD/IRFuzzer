rm -rf llvm-project/build*
rm -rf llvm-isel-afl/build*
rm -rf mutator/build*
rm -rf mapper/build*
(cd $AFL && make clean)
