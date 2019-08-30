import py
import sys

# to debug cpp code with specific instance you can run
# > gdb python3
# > r ./refpy/ instance.opb proof.log
# to debug test cases run
# > gdb python3
# r dbg.py ./tests/

py.test.cmdline.main(sys.argv)