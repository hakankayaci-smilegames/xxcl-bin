#!/bin/bash
export DEBUGINFOD_URLS=""
(
    sleep 3
    killall -SIGINT xbe-inspect
) &
gdb -q --args ./xbe-inspect/build/xbe-inspect "/home/hakan/Desktop/Sega GT 2002 (Europe) (En,Ja,Fr,De,Es,It)/default.xbe" --run << 'GDB_EOF' > gdb_spin_out3.txt 2>&1
set confirm off
set pagination off
run
x/15i 0x001a3fb0
info registers
x/8xw \$esp
quit
GDB_EOF
