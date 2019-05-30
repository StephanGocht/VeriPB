import unittest

from math import copysign

from env import refpy
from refpy.constraints import Inequality, Term


def toTerms(terms):
    return list(map(lambda x: Term(x[0], x[1]), terms))

def geq(terms, degree):
    return Inequality(toTerms(terms), degree)

class TestInequality():
    def test_saturate_signle_1(self):
        a = geq([(4, -1)], 3)
        a.saturate()
        b = geq([(3, -1)], 3)
        assert a == b

    def test_saturate_single_2(self):
        a = geq([(4, 1)], 3)
        a.saturate()
        b = geq([(3, 1)], 3)
        assert a == b

    def test_saturate_multiple_1(self):
        a = geq([(4, 1), (2,2), (5, -3), (1, -4)], 3)
        a.saturate()
        b = geq([(3, 1), (2,2), (3, -3), (1, -4)], 3)
        assert a == b

    def test_saturate_multiple_2(self):
        a = geq([(4, 1), (2,2), (5, -3), (1, -4)], 4)
        a.saturate()
        b = geq([(4, 1), (2,2), (4, -3), (1, -4)], 4)
        assert a == b

    def test_saturate_no_1(self):
        a = geq([(3, 1)], 3)
        a.saturate()
        b = geq([(3, 1)], 3)
        assert a == b

    def test_saturate_no_2(self):
        a = geq([(2, 1)], 3)
        a.saturate()
        b = geq([(2, 1)], 3)
        assert a == b

    def test_saturate_no_3(self):
        a = geq([(3, -1)], 3)
        a.saturate()
        b = geq([(3, -1)], 3)
        assert a == b

    def test_saturate_no_4(self):
        a = geq([(2, -1)], 3)
        a.saturate()
        b = geq([(2, -1)], 3)
        assert a == b

    def test_saturate_no_5(self):
        a = geq([(3, 1), (2,2), (1,-3), (2,-4), (3, -5)], 3)
        a.saturate()
        b = geq([(3, 1), (2,2), (1,-3), (2,-4), (3, -5)], 3)
        assert a == b

    def test_divide_signle_1(self):
        a = geq([(4, -1)], 3)
        a.divide(2)
        b = geq([(2, -1)], 2)
        assert a == b

    def test_divide_single_2(self):
        a = geq([(4, 1)], 3)
        a.divide(2)
        b = geq([(2, 1)], 2)
        assert a == b

    def test_divide_single_3(self):
        a = geq([(3, 1)], 4)
        a.divide(2)
        b = geq([(2, 1)], 2)
        assert a == b

    def test_divide_single_4(self):
        a = geq([(3, -1)], 4)
        a.divide(2)
        b = geq([(2, -1)], 2)
        assert a == b

    def test_divide_multiple_1(self):
        a = geq([(4, 1), (2,2), (5, -3), (1, -4)], 3)
        a.divide(2)
        b = geq([(2, 1), (1,2), (3, -3), (1, -4)], 2)
        assert a == b

    def test_divide_multiple_2(self):
        a = geq([(4, 1), (2,2), (5, -3), (1, -4)], 4)
        a.divide(4)
        b = geq([(1, 1), (1,2), (2, -3), (1, -4)], 1)
        assert a == b

    def test_divide_no_1(self):
        a = geq([(3, 1)], 3)
        a.divide(3)
        b = geq([(1, 1)], 1)
        assert a == b

    def test_divide_no_2(self):
        a = geq([(2, 1)], 4)
        a.divide(2)
        b = geq([(1, 1)], 2)
        assert a == b

    def test_divide_no_3(self):
        a = geq([(3, -1)], 3)
        a.divide(3)
        b = geq([(1, -1)], 1)
        assert a == b

    def test_divide_no_4(self):
        a = geq([(2, -1)], 4)
        a.divide(2)
        b = geq([(1, -1)], 2)
        assert a == b

    def test_divide_no_5(self):
        a = geq([(3, 1), (2,2), (1,-3), (2,-4), (3, -5)], 3)
        a.divide(1)
        b = geq([(3, 1), (2,2), (1,-3), (2,-4), (3, -5)], 3)
        assert a == b

    def test_divide_no_6(self):
        a = geq([(3, 1), (3,2), (3,-3), (3,-4), (3, -5)], 6)
        a.divide(3)
        b = geq([(1, 1), (1,2), (1,-3), (1,-4), (1, -5)], 2)
        assert a == b

    def test_add_non_canceling_1(self):
        a = geq([(1, 1)], 1)
        b = geq([(1, 1)], 1)

        a.addWithFactor(1,b)

        r = geq([(2, 1)], 2)
        assert r == a

    def test_add_non_canceling_2(self):
        a = geq([(2, 1)], 1)
        b = geq([(3, 1)], 1)

        a.addWithFactor(5,b)

        r = geq([(17, 1)], 6)
        assert r == a

    def test_add_non_canceling_3(self):
        a = geq([(2, 1)], 1)
        b = geq([(3, 2)], 1)

        a.addWithFactor(5,b)

        r = geq([(2, 1), (15,2)], 6)
        assert r == a

    def test_add_non_canceling_4(self):
        a = geq([(3, 2)], 1)
        b = geq([(2, 1)], 1)

        a.addWithFactor(5,b)

        r = geq([(10, 1), (3,2)], 6)
        assert r == a

    def test_add_non_canceling_5(self):
        a = geq([(1, 1), (1,2)], 1)
        b = geq([(1, 2), (1,3)], 1)

        a.addWithFactor(1,b)

        r = geq([(1, 1), (2,2), (1,3)], 2)
        assert r == a

    def test_add_non_canceling_6(self):
        a = geq([(1, 2), (1,3)], 1)
        b = geq([(1, 1), (1,2)], 1)

        a.addWithFactor(1,b)

        r = geq([(1, 1), (2,2), (1,3)], 2)
        assert r == a

    def test_add_non_canceling_7(self):
        a = geq([(5, 2), (1,3)], 1)
        b = geq([(1, 1), (3,2)], 1)

        a.addWithFactor(7,b)

        r = geq([(7, 1), (26,2), (1,3)], 8)
        assert r == a

    def test_add_non_canceling_8(self):
        a = geq([], 0)
        b = geq([(1, 1), (3,2)], 1)

        a.addWithFactor(2,b)

        r = geq([(2, 1), (6,2)], 2)
        assert r == a

    def test_add_non_canceling_9(self):
        a = geq([(1, 1), (3,2)], 1)
        b = geq([], 0)

        a.addWithFactor(2,b)

        r = geq([(1, 1), (3,2)], 1)
        assert r == a

    def test_add_canceling_1(self):
        a = geq([(1, -1)], 1)
        b = geq([(1,  1)], 1)

        a.addWithFactor(1,b)

        r = geq([], 1)
        assert r == a

    def test_add_canceling_2(self):
        a = geq([(1, -1)], 1)
        b = geq([(1,  1)], 1)

        a.addWithFactor(2,b)

        r = geq([(1,  1)], 2)
        assert r == a

    def test_add_canceling_3(self):
        a = geq([(2, -1)], 1)
        b = geq([(1,  1)], 1)

        a.addWithFactor(2,b)

        r = geq([], 1)
        assert r == a

    def test_add_canceling_4(self):
        a = geq([(2, -1), (1, 2)], 1)
        b = geq([(1,  -2), (1, 3)], 1)

        a.addWithFactor(2,b)

        r = geq([(2,-1), (1,-2), (2,3)], 2)
        assert r == a

    def test_add_canceling_5(self):
        a = geq([(2, -1), (1, 2), (2, -3)], 1)
        b = geq([(1,  -2), (1, 3)], 1)

        a.addWithFactor(2,b)

        r = geq([(2,-1), (1,-2)], 0)
        assert r == a

    def test_add_canceling_6(self):
        a = geq([(1, 1), (1, 2), (1, 3)], 1)
        b = geq([(1, 1), (-1, 2)], 1)

        i = Inequality()
        i.addWithFactor(1,a)
        i.addWithFactor(1,b)

        r = geq([(2,1), (1,3)], 2)
        assert r == i