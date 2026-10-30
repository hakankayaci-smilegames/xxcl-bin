#!/bin/bash
export DEBUGINFOD_URLS=""

(
    sleep 3
    killall -SIGINT xbe-inspect
) &

gdb -q --args ./xbe-inspect/build/xbe-inspect "/home/hakan/Desktop/Sega GT 2002 (Europe) (En,Ja,Fr,De,Es,It)/default.xbe" --run << 'GDB_EOF' > gdb_spin_out.txt 2>&1
set confirm off
set pagination off
handle SIGSEGV nostop noprint pass
run
info threads
thread apply all bt
thread apply all info registers
thread apply all x/15i $pc
quit
GDB_EOF
