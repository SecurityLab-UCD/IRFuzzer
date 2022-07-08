FUZZING_HOME=$(pwd)
AIE=llvm-aie
AFL=AFLplusplus

# Install llvm
if [ -d $HOME/clang+llvm ]
    cd $HOME
    wget https://github.com/llvm/llvm-project/releases/download/llvmorg-13.0.0/clang+llvm-13.0.0-x86_64-linux-gnu-ubuntu-16.04.tar.xz
    tar -xvf clang+llvm-13.0.0-x86_64-linux-gnu-ubuntu-16.04.tar.xz
    ln -s clang+llvm-13.0.0 clang+llvm
fi
export PATH=$PATH:$HOME/clang+llvm/bin

# Install AFLplusplus
git clone https://github.com/AFLplusplus/AFLplusplus.git $AFL
cd $AFL
make -j
# Put the runtime library to llvm-isel-afl for compilation. 
# This is dirty, but works before AIE is self-hosting.
cp $FUZZING_HOME/$AFL/afl-compiler-rt.o $FUZZING_HOME/llvm-isel-afl

# Build llvm-aie
git clone https://gitenterprise.xilinx.com/XRLabs/llvm-aie.git --depth=1 ../llvm-aie
# Throw llvm-aie out so it don't stuck git since we have to modify aie a bit.
ln -s ../llvm-aie llvm-aie
ln -s $FUZZING_HOME/llvm-isel-afl $FUZZING_HOME/$AIE/llvm/tools/llvm-isel-afl
mkdir -p $AIE/build-afl
cd $AIE/build-afl
cmake  -GNinja \
        -DBUILD_SHARED_LIBS=OFF \
        -DLLVM_BUILD_TOOLS=OFF \
        -DLLVM_CCACHE_BUILD=ON \
        -DLLVM_ENABLE_PROJECTS="mlir;clang;compiler-rt" \
        -DLLVM_EXPERIMENTAL_TARGETS_TO_BUILD="AIE" \
        -DLLVM_TARGETS_TO_BUILD="X86" \
        -DCMAKE_C_COMPILER=$FUZZING_HOME/$AFL/afl-clang-fast \
        -DCMAKE_CXX_COMPILER=$FUZZING_HOME/$AFL/afl-clang-fast++ \
        -DCMAKE_BUILD_TYPE=Release \
        -DLLVM_APPEND_VC_REV=OFF \
        -DLLVM_BUILD_EXAMPLES=OFF \
        -DLLVM_BUILD_RUNTIME=OFF \
        -DLLVM_INCLUDE_EXAMPLES=ON \
        -DLLVM_USE_SANITIZE_COVERAGE=OFF \
        -DLLVM_USE_SANITIZER="" \
    ../llvm
cd $FUZZING_HOME

# Prepare mutator
mkdir build
cd build
cmake ..
make -j
export AFL_CUSTOM_MUTATOR_LIBRARY=$(pwd)/mutator/libAFLCustomIRMutator.so
export AFL_CUSTOM_MUTATOR_ONLY=1
cd $FUZZING_HOME

# Run afl
# $FUZZING_HOME/$AFL/afl-fuzz -i <input> -o <output> $AIE/build-afl/bin/isel-fuzzing