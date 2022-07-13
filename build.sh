###### For dockers
#
# apt install ninja-build python3-dev cmake clang git wget ccache -y
#
# In case you need more updated cmake:
# https://askubuntu.com/questions/355565/how-do-i-install-the-latest-version-of-cmake-from-the-command-line

export FUZZING_HOME=$(pwd)
export AIE=llvm-aie
export AFL=AFLplusplus

###### Install llvm
if [ ! -d $HOME/clang+llvm ]
then
    cd $HOME
    CLANG_LLVM=clang+llvm-13.0.0-x86_64-linux-gnu-ubuntu-16.04
    wget https://github.com/llvm/llvm-project/releases/download/llvmorg-13.0.0/$CLANG_LLVM.tar.xz
    tar -xvf $CLANG_LLVM.tar.xz
    ln -s $CLANG_LLVM clang+llvm
fi
export PATH=$PATH:$HOME/clang+llvm/bin

###### Download and compile AFLplusplus
if [ ! -d $FUZZING_HOME/$AFL ]
then
    git clone https://github.com/AFLplusplus/AFLplusplus.git --depth=1 $FUZZING_HOME/$AFL
    cd $AFL
    make -j
    cd $FUZZING_HOME
fi
export AFL_LLVM_INSTRUMENT=CLASSIC

###### Download llvm-aie
if [ ! -d $FUZZING_HOME/$AIE ]
then
    git clone https://gitenterprise.xilinx.com/XRLabs/llvm-aie.git --depth=1 $FUZZING_HOME/$AIE
fi

###### Build llvm-aie
# Unfortunatelly we have to compile AIE twice. 
# `build-ninja` and `build-afl` have to be built before the whole project can be built. 
# Since the paths to LLVM is fixed in `CMakeLists.txt`

# `build-afl` is a afl-customed built with afl instrumentations so we can collect runtime info
# and report back to afl. 
# Driver also depends on `build-afl`
if [ ! -d $FUZZING_HOME/$AIE/build-afl ]
then
    mkdir -p $AIE/build-afl
    cd $AIE/build-afl
    cmake  -GNinja \
            -DBUILD_SHARED_LIBS=OFF \
            -DLLVM_BUILD_TOOLS=OFF \
            -DLLVM_CCACHE_BUILD=OFF \
            -DLLVM_ENABLE_PROJECTS="mlir" \
            -DLLVM_EXPERIMENTAL_TARGETS_TO_BUILD="AIE" \
            -DLLVM_TARGETS_TO_BUILD="X86" \
            -DCMAKE_C_COMPILER=$FUZZING_HOME/$AFL/afl-clang-fast \
            -DCMAKE_CXX_COMPILER=$FUZZING_HOME/$AFL/afl-clang-fast++ \
            -DCMAKE_BUILD_TYPE=Release \
            -DLLVM_APPEND_VC_REV=OFF \
            -DLLVM_BUILD_EXAMPLES=OFF \
            -DLLVM_BUILD_RUNTIME=OFF \
            -DLLVM_INCLUDE_EXAMPLES=OFF \
            -DLLVM_USE_SANITIZE_COVERAGE=OFF \
            -DLLVM_USE_SANITIZER="" \
        ../llvm && \
    ninja -j 50
    cd $FUZZING_HOME
fi
# Mutator depends on `build-ninja`.
# They can't depend on `build-afl` since all AFL compiled code reference to global 
# `__afl_area_ptr`(branch counting table) and `__afl_prev_loc`(edge hash)
if [ ! -d $FUZZING_HOME/$AIE/build-ninja ]
then
    mkdir -p $AIE/build-ninja
    cd $AIE/build-ninja
    cmake  -GNinja \
            -DBUILD_SHARED_LIBS=OFF \
            -DLLVM_CCACHE_BUILD=ON \
            -DLLVM_ENABLE_PROJECTS="mlir" \
            -DLLVM_EXPERIMENTAL_TARGETS_TO_BUILD="AIE" \
            -DLLVM_TARGETS_TO_BUILD="X86" \
            -DCMAKE_C_COMPILER=clang \
            -DCMAKE_CXX_COMPILER=clang++ \
            -DCMAKE_BUILD_TYPE=Release \
            -DLLVM_APPEND_VC_REV=OFF \
            -DLLVM_BUILD_EXAMPLES=OFF \
            -DLLVM_BUILD_RUNTIME=OFF \
            -DLLVM_INCLUDE_EXAMPLES=OFF \
            -DLLVM_USE_SANITIZE_COVERAGE=OFF \
            -DLLVM_USE_SANITIZER="" \
            -DCMAKE_INSTALL_PREFIX=$FUZZING_HOME/$AIE/install-aie \
        ../llvm && \
    ninja -j 50
    cd $FUZZING_HOME
fi
# Build libfuzzer as reference
if [ ! -d $FUZZING_HOME/$AIE/build-libfuzzer ]
then
    mkdir -p $AIE/build-libfuzzer
    cd $AIE/build-libfuzzer
    # Apply a patch so libfuzzer don't quit on crash
    git apply $FUZZING_HOME/libFuzzer.patch
    # Enable exceptions to use try-catch.
    cmake -GNinja \
            -DBUILD_SHARED_LIBS=OFF \
            -DLLVM_CCACHE_BUILD=ON \
            -DLLVM_ENABLE_PROJECTS="mlir" \
            -DLLVM_EXPERIMENTAL_TARGETS_TO_BUILD="AIE" \
            -DLLVM_TARGETS_TO_BUILD="X86" \
            -DCMAKE_C_COMPILER=clang \
            -DCMAKE_CXX_COMPILER=clang++ \
            -DCMAKE_BUILD_TYPE=Release \
            -DLLVM_APPEND_VC_REV=OFF \
            -DLLVM_BUILD_EXAMPLES=OFF \
            -DLLVM_BUILD_RUNTIME=OFF \
            -DLLVM_INCLUDE_EXAMPLES=OFF \
            -DLLVM_USE_SANITIZE_COVERAGE=On \
            -DLLVM_USE_SANITIZER="" \
            -DLLVM_ENABLE_RTTI=ON \
            -DLLVM_ENABLE_EH=ON \
        ../llvm && \
    ninja -j 50
    cd $FUZZING_HOME/$AIE
    git checkout llvm/lib/Support/ llvm/tools
    cd $FUZZING_HOME
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

export AFL_CUSTOM_MUTATOR_LIBRARY=$(pwd)/mutator/build/libAFLCustomIRMutator.so
export AFL_CUSTOM_MUTATOR_ONLY=1

# Run afl
# $FUZZING_HOME/$AFL/afl-fuzz -i <input> -o <output> $FUZZING_HOME/llvm-isel-afl/build/isel-fuzzing

# Kill zombie processes left over by afl.
# It will report a `no such process`, that's ok.
# That process is `grep`, which is also shown in `ps`, which died before `kill` thus doesn't exist.
# kill -9 $(ps aux | grep isel-fuzzing | awk '{print $2}')