FROM ubuntu:20.04

RUN apt-get update && \
    apt-get -y upgrade 
ENV DEBIAN_FRONTEND noninteractive
RUN apt-get install -y -q git build-essential wget zlib1g-dev cmake python3 python3-pip ninja-build ccache && \
    apt-get clean

ENV FUZZING_HOME=/IRFuzzer

RUN mkdir -p /$FUZZING_HOME
COPY . /$FUZZING_HOME
WORKDIR /$FUZZING_HOME

ENV LLVM=llvm-project
ENV AFL=AFLplusplus
ENV PATH=$PATH:$HOME/clang+llvm/bin
ENV AFL_LLVM_INSTRUMENT=CLASSIC

# Run the bulk LLVM download to cache it in docker image.
RUN cd $HOME; \
    export CLANG_LLVM=clang+llvm-14.0.0-x86_64-linux-gnu-ubuntu-18.04; \
    wget https://github.com/llvm/llvm-project/releases/download/llvmorg-14.0.0/$CLANG_LLVM.tar.xz; \
    tar -xvf $CLANG_LLVM.tar.xz; \
    ln -s $CLANG_LLVM clang+llvm

# TODO: Eventually we want to move everythign in build.sh to here.
RUN ./build.sh