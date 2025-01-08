#!/bin/bash

set -e

BASEDIR="$(dirname $0)"/../
cd "$BASEDIR"

(
    cd build
    make -j
)

SC="$(pwd)/build/sc"

for dir in test_data/sci-scripts/*; do
    if [ ! -d "$dir" ]; then
        continue
    fi
    echo "Compiling $dir"
    (
        cd "$dir"
        "$SC" src/*.sc
    )
done
