make clean
make
mkdir -p build/programs

./build/converter asm/program_v1.asm build/programs/main.bin
./build/vm32_host build/programs > build/hot_reload.log 2>&1 &
PID=$!

sleep 3
./build/converter asm/program_v2.asm build/programs/main.tmp.bin
mv -f build/programs/main.tmp.bin build/programs/main.bin

sleep 3
kill "$PID" || true
wait "$PID" || true
grep -n "Hot-reload applied" build/hot_reload.log