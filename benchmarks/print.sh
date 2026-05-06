#!/bin/bash

for file in *.txt; do
    echo "===== $file ====="
    head -n 1 "$file"
    echo
done
