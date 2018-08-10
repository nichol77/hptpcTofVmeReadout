#!/bin/bash

if [[ $1 = "" ]]; then
    echo "`basename $0` <run> "
    exit 1
fi

run=${1}
for rawfile in /unix/dune/hptpctof/run${run}/raw*; do
    echo $rawfile ${rawfile/raw/parsed}
    ./decodeRawFile $rawfile ${rawfile/raw/parsed}
done
