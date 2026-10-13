# run with:  gdb -q -x step.gdb bin/add
# walks through main -> add -> back to main, printing the registers that define the stack
set disassembly-flavor intel
set pagination off
break main
run

echo \n=== inside main, before any locals are set ===\n
info registers rip rsp rbp
x/6i $rip

echo \n=== step into add() ===\n
break add
continue
echo \n--- add's frame: rbp is the new base, the caller's rbp + return address sit just above ---\n
info registers rip rsp rbp
info frame
echo \n--- raw stack: [rbp] = saved caller rbp, [rbp+8] = return address into main ---\n
x/2gx $rbp
echo \n--- the instructions of add ---\n
disassemble add

echo \n=== finish add and land back in main ===\n
finish
info registers rip rsp rbp eax

continue
quit
