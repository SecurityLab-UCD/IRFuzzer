#!/bin/bash

I=0
SECS=86400
OUT_DIR=$1

echo "Generating C files for $SECS seconds to $OUT_DIR..."
sleep 5

END=$((SECONDS+SECS))

while [ $SECONDS -lt $END ]; do
    I=$((I+1))
    csmith -o $OUT_DIR/$I.c
done

echo "Generated $I C files."
