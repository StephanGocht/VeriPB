import py
import sys

# to debug cpp code with specific instance you can run
# > gdb python3
# > r ./veripb/ instance.opb proof.log
# if you want to use asan prepend the following if compiled with gcc
# LD_PRELOAD=$(gcc -print-file-name=libasan.so)

# to debug test cases run
# > gdb python3
# r dbg.py ./tests/


py.test.cmdline.main(sys.argv)