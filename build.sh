#!/bin/bash

mkdir build

cmake -B ./build -S ./
cd build
make

rm -rf host*autogen; mv host* ../
rm -rf client*autogen; mv client* ../

cd ..
rm -r build