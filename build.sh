#!/bin/bash

# Quit on any error.
set -e

# Path to this directory
export FUZZING_HOME=$(pwd)
# The LLVM you want to fuzz
export LLVM=llvm-project
export AFL=AFLplusplus
export PATH=$PATH:$HOME/clang+llvm/bin

###### Compile AFLplusplus
cd $FUZZING_HOME/$AFL; make -j; cd $FUZZING_HOME
export AFL_LLVM_INSTRUMENT=CLASSIC

###### Build LLVM & AIE
# Unfortunatelly we have to compile LLVM twice. 
# `build-afl` is the build to be fuzzed.
# `build-release` is the dependency of mutator
# The paths to both LLVM is fixed in `CMakeLists.txt`

# `build-afl` is a afl-customed built with afl instrumentations so we can collect runtime info
# and report back to afl. 
# Driver also depends on `build-afl`
if [ ! -d $FUZZING_HOME/$LLVM/build-afl ]
then
    mkdir -p $LLVM/build-afl
    cd $LLVM/build-afl
    cmake  -GNinja \
            -DBUILD_SHARED_LIBS=OFF \
            -DLLVM_BUILD_TOOLS=ON \
            -DLLVM_CCACHE_BUILD=OFF \
            -DLLVM_TARGETS_TO_BUILD="AArch64" \
            -DLLVM_BUILD_TOOLS=OFF \
            -DCMAKE_C_COMPILER=$FUZZING_HOME/$AFL/afl-clang-fast \
            -DCMAKE_CXX_COMPILER=$FUZZING_HOME/$AFL/afl-clang-fast++ \
            -DCMAKE_BUILD_TYPE=Release \
            -DLLVM_APPEND_VC_REV=OFF \
            -DLLVM_BUILD_EXAMPLES=OFF \
            -DLLVM_BUILD_RUNTIME=OFF \
            -DLLVM_INCLUDE_EXAMPLES=OFF \
            -DLLVM_USE_SANITIZE_COVERAGE=OFF \
            -DLLVM_USE_SANITIZER="" \
        ../llvm
    cd $FUZZING_HOME
fi
cd $LLVM/build-afl; ninja -j $(nproc --all); cd ../..

# Mutator depends on `build-release`.
# They can't depend on `build-afl` since all AFL compiled code reference to global 
# `__afl_area_ptr`(branch counting table) and `__afl_prev_loc`(edge hash)
if [ ! -d $FUZZING_HOME/$LLVM/build-release ]
then
    mkdir -p $LLVM/build-release
    cd $LLVM/build-release
    cmake  -GNinja \
            -DBUILD_SHARED_LIBS=ON \
            -DLLVM_CCACHE_BUILD=ON \
            -DLLVM_TARGETS_TO_BUILD="AArch64" \
            -DLLVM_BUILD_TOOLS=OFF \
            -DCMAKE_C_COMPILER=clang \
            -DCMAKE_CXX_COMPILER=clang++ \
            -DCMAKE_BUILD_TYPE=Release \
        ../llvm
    cd $FUZZING_HOME
fi
cd $LLVM/build-release; ninja -j $(nproc --all); cd ../..

# Don't build debug build in docker.
if [ ! -f /.dockerenv ]; then
    # Mutator depends on `build-release`.
    # They can't depend on `build-afl` since all AFL compiled code reference to global 
    # `__afl_area_ptr`(branch counting table) and `__afl_prev_loc`(edge hash)
    if [ ! -d $FUZZING_HOME/$LLVM/build-debug ]
    then
        mkdir -p $LLVM/build-debug
        cd $LLVM/build-debug
        cmake  -GNinja \
                -DBUILD_SHARED_LIBS=ON \
                -DLLVM_CCACHE_BUILD=ON \
                -DLLVM_TARGETS_TO_BUILD="AArch64" \
                -DLLVM_BUILD_TOOLS=ON \
                -DCMAKE_C_COMPILER=clang \
                -DCMAKE_CXX_COMPILER=clang++ \
                -DCMAKE_BUILD_TYPE=Debug \
            ../llvm
        cd $FUZZING_HOME
    fi
    cd $LLVM/build-debug; ninja -j $(nproc --all); cd ../..
fi

###### Compile driver.
# Driver has to be compiled by `afl-clang-fast`, so the `afl_init` is inserted before `main`
mkdir -p llvm-isel-afl/build
cd llvm-isel-afl/build
cmake -GNinja \
        -DCMAKE_C_COMPILER=$FUZZING_HOME/$AFL/afl-clang-fast \
        -DCMAKE_CXX_COMPILER=$FUZZING_HOME/$AFL/afl-clang-fast++ \
    .. && \
ninja -j 4
cd $FUZZING_HOME

###### Compile mutator.
mkdir -p mutator/build
cd mutator/build
cmake -GNinja .. && ninja -j $(nproc --all)
cd $FUZZING_HOME
mkdir -p mutator/build-debug
cd mutator/build-debug
cmake -GNinja .. -DCMAKE_BUILD_TYPE=Debug && ninja -j $(nproc --all)
cd $FUZZING_HOME

### We are using `scripts/fuzz.py` now.
# Tell AFL++ to only use our mutator
# export AFL_CUSTOM_MUTATOR_ONLY=1
# Tell AFL++ Where our mutator is
# export AFL_CUSTOM_MUTATOR_LIBRARY=$FUZZING_HOME/mutator/build/libAFLCustomIRMutator.so

if [ ! -f $FUZZING_HOME/seeds/*.ll ]
then
    echo "Preparing seeds..."
    cd seeds.ll
    for I in *.ll; do
        $FUZZING_HOME/llvm-project/build-release/bin/llvm-as $I
    done
    mkdir -p ../seeds
    mv *.bc ../seeds
    echo "Done."
    cd $FUZZING_HOME
fi
# Run afl
# $FUZZING_HOME/$AFL/afl-fuzz -i <input> -o <output> $FUZZING_HOME/llvm-isel-afl/build/isel-fuzzing

# Kill zombie processes left over by afl.
# It will report a `no such process`, that's ok.
# That process is `grep`, which is also shown in `ps`, which died before `kill` thus doesn't exist.
# kill -9 $(ps aux | grep isel-fuzzing | awk '{print $2}')

###### Compile mapper.
mkdir -p mapper/build
cd mapper/build
cmake -GNinja .. && ninja -j 4
cd $FUZZING_HOME

build_pl(){
if [ ! -d $FUZZING_HOME/$LLVM/build-pl ]
then
    ###### Generate all pattern lookup tables (JSONs)
    cd $LLVM
    # TODO: Currently this approach will for all (debug, afl++, release, pl) builds to rebuild
    # because these files in the pattern lookup changed. That's ~1000 tasks in each build, which
    # is a waste of time. Think of better options in the future.
    # One idea: git apply <patch> and then git checkout.
    git fetch origin pattern-lookup
    git cherry-pick -n origin/pattern-lookup
    mkdir -p build-pl
    cd build-pl
    cmake  -GNinja \
            -DBUILD_SHARED_LIBS=ON \
            -DLLVM_CCACHE_BUILD=ON \
            -DLLVM_TARGETS_TO_BUILD="AArch64" \
            -DLLVM_BUILD_TOOLS=OFF \
            -DCMAKE_C_COMPILER=clang \
            -DCMAKE_CXX_COMPILER=clang++ \
            -DCMAKE_BUILD_TYPE=Release \
        ../llvm
    ninja -j $(nproc --all) llvm-tblgen llc

    cd "$FUZZING_HOME/$LLVM/llvm/lib/Target"
    targets=$(find * -maxdepth 0 -type d -print)
    outdir="$FUZZING_HOME/$LLVM/build-pl/pattern-lookup"
    cd "$FUZZING_HOME/$LLVM"
    mkdir -p "$outdir"
    for target in $targets; do
        echo "Generating pattern lookup table for $target..."
        target_td=$target.td
        if [[ $target == "PowerPC" ]]; then
            target_td="PPC.td"
        fi
        "$FUZZING_HOME/$LLVM/build-pl/bin/llvm-tblgen" -gen-dag-isel -pattern-lookup "$outdir/$target.json" \
            -I./llvm/lib/Target/$target -I./build-pl/include -I./llvm/include -I./llvm/lib/Target \
            ./llvm/lib/Target/$target/$target_td -o "$outdir/$target.inc" -d /dev/null &
    done
    wait

    git reset --hard
    cd $FUZZING_HOME
fi
}

# build_pl