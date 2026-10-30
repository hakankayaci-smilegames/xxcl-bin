#!/bin/bash
export DEBUGINFOD_URLS=""

# Start the emulator in the background
./xbe-inspect/build/xbe-inspect "/home/hakan/Desktop/Sega GT 2002 (Europe) (En,Ja,Fr,De,Es,It)/default.xbe" --run > run_output.log 2>&1 &
PID=$!

echo "Emulator started with PID $PID. Waiting 3 seconds for spin-loop..."
sleep 3

echo "Attaching GDB to PID $PID..."
gdb -q -ex "set confirm off" \
    -p $PID \
    -ex "thread apply all bt" \
    -ex "thread apply all info registers eip eax ebx ecx edx" \
    -ex "thread apply all x/2i \$eip" \
    -ex "quit" > gdb_output.txt 2>&1

echo "Killing emulator..."
kill -9 $PID
