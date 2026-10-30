#!/bin/bash
export DEBUGINFOD_URLS=""

# Create a GDB script file
cat << 'GDB_EOF' > gdb_script.txt
set confirm off
handle SIGSEGV nostop noprint pass
run
x/5i 0x1a1c40
quit
GDB_EOF

# Run GDB in the background
gdb -q -x gdb_script.txt --args ./xbe-inspect/build/xbe-inspect "/home/hakan/Desktop/Sega GT 2002 (Europe) (En,Ja,Fr,De,Es,It)/default.xbe" --run > gdb_output.txt 2>&1 &
GDB_PID=$!

sleep 4

EMU_PID=$(pgrep -P $GDB_PID)
kill -SIGINT $EMU_PID
sleep 1

kill -9 $GDB_PID $EMU_PID 2>/dev/null
