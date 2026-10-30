#!/bin/bash
export DEBUGINFOD_URLS=""

(
    sleep 4
    killall -SIGINT xbe-inspect
) &

gdb -q --args ./xbe-inspect/build/xbe-inspect "/home/hakan/Desktop/Sega GT 2002 (Europe) (En,Ja,Fr,De,Es,It)/default.xbe" --run << 'GDB_EOF' > gdb_out.txt 2>&1
set confirm off
set pagination off
handle SIGSEGV nostop noprint pass
run
thread 9
x/10i 0x1a3234
quit
GDB_EOF
