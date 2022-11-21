# Quit on any error.
set -e

###### For dockers
#
# apt install ninja-build python3-dev cmake clang git wget ccache -y
#
# In case you need more updated cmake:
# https://askubuntu.com/questions/355565/how-do-i-install-the-latest-version-of-cmake-from-the-command-line

# This is just to make sure we use correct cmake and ninja.
# cmake() { /group/xrlabs/tools/x86_64_Ubuntu18/bin/cmake $@; }
# ninja() { /group/xrlabs/tools/x86_64_Ubuntu18/bin/ninja $@; }

# Path to this directory
export FUZZING_HOME=$(pwd)
# The LLVM you want to fuzz
export LLVM=llvm-project
export AFL=AFLplusplus

###### Install llvm
if [ ! -d $HOME/clang+llvm ]
then
    cd $HOME
    CLANG_LLVM=clang+llvm-14.0.0-x86_64-linux-gnu-ubuntu-18.04
    wget https://github.com/llvm/llvm-project/releases/download/llvmorg-14.0.0/$CLANG_LLVM.tar.xz
    tar -xvf $CLANG_LLVM.tar.xz
    mv $CLANG_LLVM clang+llvm14
    ln -s clang+llvm14 clang+llvm
fi
export PATH=$PATH:$HOME/clang+llvm/bin

###### Download and compile AFLplusplus
if [ ! -d $FUZZING_HOME/$AFL ]
then
    git clone https://github.com/DataCorrupted/AFLplusplus.git --branch=isel --depth=1 $FUZZING_HOME/$AFL
    cd $AFL; make -j; cd $FUZZING_HOME
fi
export AFL_LLVM_INSTRUMENT=CLASSIC

###### Download llvm-project
if [ ! -d $FUZZING_HOME/$LLVM ]
then
    git clone https://github.com/DataCorrupted/llvm-project.git --depth=1 $FUZZING_HOME/$LLVM
fi

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
            -DLLVM_CCACHE_BUILD=ON \
            -DLLVM_ENABLE_PROJECTS="mlir" \
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
cmake -GNinja .. && ninja -j 4
cd $FUZZING_HOME

# Tell AFL++ to only use our mutator
export AFL_CUSTOM_MUTATOR_ONLY=1
# Tell AFL++ Where our mutator is
export AFL_CUSTOM_MUTATOR_LIBRARY=$FUZZING_HOME/mutator/build/libAFLCustomIRMutator.so

# Run afl
# $FUZZING_HOME/$AFL/afl-fuzz -i <input> -o <output> $FUZZING_HOME/llvm-isel-afl/build/isel-fuzzing

# Kill zombie processes left over by afl.
# It will report a `no such process`, that's ok.
# That process is `grep`, which is also shown in `ps`, which died before `kill` thus doesn't exist.
# kill -9 $(ps aux | grep isel-fuzzing | awk '{print $2}')

# cat fuzzing-wo-shadow*/default/fuzzer_stats | grep shadow_cvg ; cat fuzzing-w-shadow*/default/fuzzer_stats | grep shadow_cvg

# for I in {0..2}
# do
#     screen -S fuzzing-dagisel-$I -dm bash -c "export MATCHER_TABLE_SIZE=22600; $FUZZING_HOME/$AFL/afl-fuzz -i ~/fuzzing_LLVM/seeds/ -o ~/fuzzing_LLVM/3.fuzzing.vec++/fuzzing-dagisel-$I  -w /home/yuyangr/aflplusplus-isel/llvm-isel-afl/build/isel-fuzzing; bash"
#     screen -S fuzzing-globalisel-$I -dm bash -c "export GLOBAL_ISEL=1; export MATCHER_TABLE_SIZE=13780; $FUZZING_HOME/$AFL/afl-fuzz -i ~/fuzzing_LLVM/seeds/ -o ~/fuzzing_LLVM/3.fuzzing.vec++/fuzzing-globalisel-$I  -w /home/yuyangr/aflplusplus-isel/llvm-isel-afl/build/isel-fuzzing; bash"
# done

# screen -S dagisel -dm bash -c "export TRIPLE=aie; export MATCHER_TABLE_SIZE=22600; $FUZZING_HOME/$AFL/afl-fuzz -i ~/fuzzing_AIE/seeds/ -o ~/fuzzing_AIE/5.aie16.0-isel/dagisel  -w /home/yuyangr/aflplusplus-isel/llvm-isel-afl/build/isel-fuzzing; bash"
# screen -S globalisel -dm bash -c "export TRIPLE=aie; export GLOBAL_ISEL=1; export MATCHER_TABLE_SIZE=13780; $FUZZING_HOME/$AFL/afl-fuzz -i ~/fuzzing_AIE/seeds/ -o ~/fuzzing_AIE/5.aie16.0-isel/globalisel  -w /home/yuyangr/aflplusplus-isel/llvm-isel-afl/build/isel-fuzzing; bash"