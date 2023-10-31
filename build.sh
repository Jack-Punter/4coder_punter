#!/bin/bash

set -e

../bin/buildsuper_x64-linux.sh 4coder_fleury.cpp release
cp custom_4coder.so ../../custom_4coder.so
