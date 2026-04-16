#!/bin/bash

set -e

echo "=== BUILD ==="
make

echo "=== BUILD TEST ==="
make build/bin/hot_reload_test

echo "=== CONVERT PROGRAMS ==="
./build/converter asm/program_v1.asm build/bin/program.bin

echo "=== RUN TEST ==="
./build/bin/hot_reload_test &
TEST_PID=$!

sleep 2

echo "=== HOT RELOAD ==="
./build/converter asm/program_v2.asm build/bin/program.bin

sleep 3

kill $TEST_PID

echo "=== DONE ==="