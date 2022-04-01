#!/bin/bash

pushd ../../BufMgr/src/
rm libbm.a
make clean
make
ar -cvq libbm.a buf.o
popd 
cp ../../BufMgr/src/libbm.a ../lib/
make clean
make
./hfpage >& mine.txt
