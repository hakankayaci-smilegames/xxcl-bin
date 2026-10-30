#!/bin/bash
export DEBUGINFOD_URLS=""

(
    sleep 3
    killall -SIGINT xbe-inspect
) &

gdb -q --args ./xbe-inspect/build/xbe-inspect "/home/hakan/Desktop/Sega GT 2002 (Europe) (En,Ja,Fr,De,Es,It)/default.xbe" --run << 'GDB_EOF' > gdb_spin_out2.txt 2>&1
set confirm off
set pagination off
handle SIGSEGV nostop noprint pass
run
thread 9
x/15i 0x16886c
info registers
quit
GDB_EOF
