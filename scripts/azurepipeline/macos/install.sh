#!/bin/bash

#Install Build Tools
brew install bison
brew install freeimage
brew install pyenv

eval "$(pyenv init -)"

pyenv install 3.5.3
pyenv global 3.5.3
pyenv shell 3.5.3

sudo pip3 install -U numpy
sudo pip3 install pillow
sudo pip3 install pyside2
