import sys
import cppimport

pybindings = cppimport.imp("veripb.optimized.pybindings")
sys.modules[__name__ + ".constraints"] = pybindings.constraints
sys.modules[__name__ + ".parsing"] = pybindings.parsing