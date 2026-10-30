#!/bin/bash
export DEBUGINFOD_URLS=""

cat << 'GDB_EOF' > gdb_script.txt
set confirm off
handle SIGSEGV nostop noprint pass
run
shell sleep 3
interrupt
x/10i 0x1a1c40
quit
GDB_EOF

gdb -q -x gdb_script.txt --args ./xbe-inspect/build/xbe-inspect "/home/hakan/Desktop/Sega GT 2002 (Europe) (En,Ja,Fr,De,Es,It)/default.xbe" --run > dump_loop_out.txt 2>&1
