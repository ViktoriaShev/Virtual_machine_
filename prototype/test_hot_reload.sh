#!/bin/bash
set -euo pipefail

DIR=build/programs
mkdir -p "$DIR"

make clean
make

./build/converter asm/program_v1.asm "$DIR/main.bin"

./build/vm32_host "$DIR" > build/hot_reload.log 2>&1 &
PID=$!

sleep 3

./build/converter asm/program_v2.asm "$DIR/main.tmp.bin"
mv -f "$DIR/main.tmp.bin" "$DIR/main.bin"

sleep 3

kill "$PID" || true
wait "$PID" || true

grep -n "Hot-reload applied" build/hot_reload.log
grep -n "Combined program hash" build/hot_reload.log