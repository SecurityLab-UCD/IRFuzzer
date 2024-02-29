# Quit on any error.
set -e

###### For dockers
#
# apt install ninja-build python3-dev cmake clang git wget ccache -y
#
# In case you need more updated cmake:
# https://askubuntu.com/questions/355565/how-do-i-install-the-latest-version-of-cmake-from-the-command-line

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
    rm $CLANG_LLVM.tar.xz
    mv $CLANG_LLVM clang+llvm14
    ln -s clang+llvm14 clang+llvm
fi
export PATH=$PATH:$HOME/clang+llvm/bin

cd $FUZZING_HOME
###### Download submodules
git submodule init
git submodule update

###### Install python dependencies
pip3 install -r scripts/requirements.txt