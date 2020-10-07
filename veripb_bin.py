import sys
import os
sys.path.insert(0, os.path.abspath(os.path.dirname(__file__), '..'))

import veripb
import veripb.optimized.pybindings

if __name__ == '__main__':
    veripb.run_cmd_main()