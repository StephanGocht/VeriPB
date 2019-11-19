import unittest
from env import veripb

import cppimport
constraints = cppimport.imp("veripb.optimized.constraints")

Inequality = constraints.CppInequality

def getParser():
    return True

# Inequality.foo = getParser

def geq(terms, degree):
    coeffs, lits = zip(*terms) if len(terms) > 0 else ([],[])
    return Inequality(coeffs, lits, degree)


class TestCpp(unittest.TestCase):
    def testCppTrue(self):
        assert constraints.nonzero([1,2,3,4])

    def testCppFalse(self):
        assert not constraints.nonzero([1,2,0,3,4])

    def testConstruction_1(self):
        a = Inequality([1,2], [1,2], 1)
        assert not a.isContradiction()

    def testConstruction_2(self):
        a = Inequality([1,2], [1,2], 4)
        assert a.isContradiction()

#     def testEQ(self):
#         a = Inequality([1,1], [1,2], 4)
#         b = Inequality([1,1], [2,1], 4)
#         # assert a.__eq__(b)
#         assert a == b


#     def test_fat(self):
#         a = geq([(1, 1), (3,2)], 1)
#         b = geq([(1, 1), (3,2)], 1)

#         a.expand()
#         a.contract()

#         assert a == b

#     def test_add_canceling_6(self):
#         a = geq([(1, 1), (1, 2), (1, 3)], 1)
#         b = geq([(1, 1), (1, -2)], 2)

#         i = geq([], 0)
#         a = a.copy()
#         a = a.multiply(1)
#         i = i.add(a)
#         b = b.copy()
#         b = b.multiply(1)
#         i = i.add(b)

#         r = geq([(2,1), (1,3)], 2)
#         assert r == i

# if __name__ == '__main__':
#     unittest.main()