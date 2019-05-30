import sys
import os
sys.path.insert(0, os.path.abspath(os.path.join(os.path.dirname(__file__), '..')))

import refpy

if __name__ == '__main__':
    refpy.run_cmd_main()