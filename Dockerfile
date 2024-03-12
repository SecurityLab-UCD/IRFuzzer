FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && \
    apt-get -y upgrade && \
    apt-get install -y -q git build-essential wget zlib1g-dev cmake python3 python3-pip ninja-build ccache && \
    apt-get clean

ENV FUZZING_HOME=/IRFuzzer
RUN git clone https://github.com/SecurityLab-UCD/IRFuzzer.git $FUZZING_HOME -b irfuzzer-alive
WORKDIR $FUZZING_HOME

ENV LLVM=llvm-project
ENV AFL=AFLplusplus
ENV PATH="${PATH}:/clang+llvm/bin"
ENV AFL_LLVM_INSTRUMENT=CLASSIC

RUN ./init.sh

RUN ./build.sh

ARG IRFUZZER_COMMIT
RUN git fetch origin && git checkout $IRFUZZER_COMMIT
RUN ./init.sh && ./build.sh