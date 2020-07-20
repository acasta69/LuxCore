#!/bin/bash

#Fetch Artifacts
wget https://github.com/LuxCoreRender/MacOSCompileDeps/releases/download/luxcorerender_v2.4beta4/MacDistFiles.tar.gz
tar xzf MacDistFiles.tar.gz

# Set Environment Variables
export PATH="/usr/local/opt/bison/bin:/usr/local/bin:$PATH"
DEPS_SOURCE=`pwd`/macos
eval "$(pyenv init -)"

#==========================================================================
# Compiling Unified version"
#==========================================================================

mkdir build
pushd  build
cmake -DOSX_DEPENDENCY_ROOT=$DEPS_SOURCE -DCMAKE_BUILD_TYPE=Release ..
make
popd
