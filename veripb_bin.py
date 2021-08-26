# this is a dummy file for creating a distribution with pyinstaller
# but should also work for running veripb
import sys
import os
sys.path.insert(0, os.path.abspath(os.path.dirname(__file__)))

import veripb

# add explicit import of pybindings, somehow pyinstaller is missing it
# otherwise
import veripb.optimized.pybindings

if __name__ == '__main__':
    sys.exit(veripb.run_cmd_main())