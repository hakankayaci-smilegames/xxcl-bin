#!/bin/bash
gdb -q --args ./xbe-inspect/build/xbe-inspect "/home/hakan/Desktop/Sega GT 2002 (Europe) (En,Ja,Fr,De,Es,It)/default.xbe" --run << 'GDB_EOF' > dump_out.txt 2>&1
set confirm off
set pagination off
run &
sleep 2
x/20i 0x815b0
quit
GDB_EOF
cat dump_out.txt | tail -n 30
