#!/bin/bash

#automated script build for macos 10.13 should compile and patch with dependencies in root/macos
# set python and bison env *
eval "$(pyenv init -)"
pyenv shell 3.7.4
export PATH="/usr/local/opt/bison/bin:/usr/local/bin:$PATH"

# build luxcore
rm -rf build

mkdir build
pushd  build
cmake -DCMAKE_BUILD_TYPE=Release -DLUXRAYS_ENABLE_CUDA=1 -Wno-dev ..
make -j8
popd
