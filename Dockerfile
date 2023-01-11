FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && \
    apt-get -y upgrade && \
    apt-get install -y -q git build-essential wget zlib1g-dev cmake python3 python3-pip ninja-build ccache && \
    apt-get clean

ENV FUZZING_HOME=/IRFuzzer
WORKDIR $FUZZING_HOME
COPY . $FUZZING_HOME

ENV LLVM=llvm-project
ENV AFL=AFLplusplus
ENV PATH="${PATH}:/clang+llvm/bin"
ENV AFL_LLVM_INSTRUMENT=CLASSIC

RUN CLANG_LLVM=clang+llvm-14.0.0-x86_64-linux-gnu-ubuntu-18.04 && \
    wget --no-verbose --show-progress --progress=dot:mega https://github.com/llvm/llvm-project/releases/download/llvmorg-14.0.0/$CLANG_LLVM.tar.xz && \
    tar -xvf $CLANG_LLVM.tar.xz -C / && \
    mv /$CLANG_LLVM /clang+llvm && \
    rm $CLANG_LLVM.tar.xz

RUN git clone https://github.com/SecurityLab-UCD/AFLplusplus.git --branch=isel --depth=1 $AFL && \
    cd $AFL && \
    make -j

RUN git clone --branch irfuzzer-0.1 https://github.com/SecurityLab-UCD/llvm-project.git --depth=1 $LLVM

RUN mkdir -p $LLVM/build-afl && \
    cd $LLVM/build-afl && \
    cmake \
    -GNinja \
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
    ../llvm && \
    ninja -j $(nproc --all)

RUN mkdir -p $LLVM/build-release && \
    cd $LLVM/build-release && \
    cmake \
    -GNinja \
    -DBUILD_SHARED_LIBS=ON \
    -DLLVM_CCACHE_BUILD=ON \
    -DCMAKE_C_COMPILER=clang \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_BUILD_TYPE=Release \
    ../llvm && \
    ninja -j $(nproc --all)

RUN mkdir -p llvm-isel-afl/build && \
    cd llvm-isel-afl/build && \
    cmake \
    -GNinja \
    -DCMAKE_C_COMPILER=$FUZZING_HOME/$AFL/afl-clang-fast \
    -DCMAKE_CXX_COMPILER=$FUZZING_HOME/$AFL/afl-clang-fast++ \
    .. && \
    ninja -j $(nproc --all)

RUN mkdir -p mutator/build && \
    cd mutator/build && \
    cmake -GNinja .. && \
    ninja -j $(nproc --all)
