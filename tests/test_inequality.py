import unittest

from math import copysign

from env import veripb

from veripb.constraints import Term
from veripb.constraints import CppIneqFactory as IneqFactory

ineqFactory = IneqFactory()

def toTerms(terms):
    return list(map(lambda x: Term(x[0], x[1]), terms))

def geq(terms, degree):
    return ineqFactory.fromTerms(toTerms(terms), degree)

def clause(literals):
    return Inequality([Term(1, l) for l in literals] , 1)

class TestInequality(unittest.TestCase):

    def test_equals_1(self):
        a = geq([(4, -1)], 3)
        b = geq([(4, -1)], 3)
        assert a == b

    def test_equals_2(self):
        a = geq([(4, -1), (3,2)], 3)
        b = geq([(3,2), (4, -1)], 3)
        assert a == b

    def test_implies_1(self):
        a = geq([(4, -1), (3,2)], 3)
        b = geq([(3,2), (4, -1), (3, 3)], 3)
        assert a.implies(b)

    def test_implies_2(self):
        a = geq([(4, -1), (3,2)], 4)
        b = geq([(3,2), (4, -1)], 3)
        assert a.implies(b)

    def test_implies_3(self):
        a = geq([(4, -1), (3,2)], 4)
        b = geq([(3,2), (4, -1), (3, 3)], 3)
        assert a.implies(b)

    def test_implies_4(self):
        a = geq([(4, -1), (3,2)], 4 + 1 + 3)
        b = geq([(2,2), (2, 1), (3, 3)], 3)
        assert a.implies(b)

    def test_implies_4_f(self):
        a = geq([(4, -1), (3,2)], 4 + 1 + 3)
        b = geq([(2,2), (2, 1), (3, 3)], 4)
        assert (not a.implies(b))

    def test_implies_5(self):
        a = geq([(5, 1), (1,2), (1,3)], 3)
        b = geq([(1,1)], 1)
        assert a.implies(b)

    def test_implies_5_f(self):
        a = geq([(5, 1), (1,2), (1,3), (1,4)], 3)
        b = geq([(1,1)], 1)
        assert(not a.implies(b))

    def test_implies_6(self):
        a = geq([(1, 1), (5,2), (5,3), (1,4)], 3)
        b = geq([(1,2), (1,3)], 1)
        assert a.implies(b)

    def test_implies_6_f(self):
        a = geq([(2, 1), (5,2), (5,3), (1,4)], 3)
        b = geq([(1,2), (1,3)], 1)
        assert(not a.implies(b))

    def test_implies_7_f(self):
        a = geq([(2,-1), (5,2), (5,3), (1,4)], 3)
        b = geq([(1,2), (1,3)], 1)
        assert(not a.implies(b))


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

        b = b.copy()
        b = b.multiply(1)
        a = a.add(b)

        r = geq([(2, 1)], 2)
        assert r == a

    def test_add_non_canceling_2(self):
        a = geq([(2, 1)], 1)
        b = geq([(3, 1)], 1)

        b = b.copy()
        b = b.multiply(5)
        a = a.add(b)

        r = geq([(17, 1)], 6)
        assert r == a

    def test_add_non_canceling_3(self):
        a = geq([(2, 1)], 1)
        b = geq([(3, 2)], 1)

        b = b.copy()
        b = b.multiply(5)
        a = a.add(b)

        r = geq([(2, 1), (15,2)], 6)
        assert r == a

    def test_add_non_canceling_4(self):
        a = geq([(3, 2)], 1)
        b = geq([(2, 1)], 1)

        b = b.copy()
        b = b.multiply(5)
        a = a.add(b)

        r = geq([(10, 1), (3,2)], 6)
        assert r == a

    def test_add_non_canceling_5(self):
        a = geq([(1, 1), (1,2)], 1)
        b = geq([(1, 2), (1,3)], 1)

        b = b.copy()
        b = b.multiply(1)
        a = a.add(b)

        r = geq([(1, 1), (2,2), (1,3)], 2)
        assert r == a

    def test_add_non_canceling_6(self):
        a = geq([(1, 2), (1,3)], 1)
        b = geq([(1, 1), (1,2)], 1)

        b = b.copy()
        b = b.multiply(1)
        a = a.add(b)

        r = geq([(1, 1), (2,2), (1,3)], 2)
        assert r == a

    def test_add_non_canceling_7(self):
        a = geq([(5, 2), (1,3)], 1)
        b = geq([(1, 1), (3,2)], 1)

        b = b.copy()
        b = b.multiply(7)
        a = a.add(b)

        r = geq([(7, 1), (26,2), (1,3)], 8)
        assert r == a

    def test_add_non_canceling_8(self):
        a = geq([], 0)
        b = geq([(1, 1), (3,2)], 1)

        b = b.copy()
        b = b.multiply(2)
        a = a.add(b)

        r = geq([(2, 1), (6,2)], 2)
        assert r == a

    def test_add_non_canceling_9(self):
        a = geq([(1, 1), (3,2)], 1)
        b = geq([], 0)

        b = b.copy()
        b = b.multiply(2)
        a = a.add(b)

        r = geq([(1, 1), (3,2)], 1)
        assert r == a

    def test_add_canceling_1(self):
        a = geq([(1, -1)], 1)
        b = geq([(1,  1)], 1)

        b = b.copy()
        b = b.multiply(1)
        a = a.add(b)

        r = geq([], 1)
        assert r == a

    def test_add_canceling_2(self):
        a = geq([(1, -1)], 1)
        b = geq([(1,  1)], 1)

        b = b.copy()
        b = b.multiply(2)
        a = a.add(b)

        r = geq([(1,  1)], 2)
        assert r == a

    def test_add_canceling_3(self):
        a = geq([(2, -1)], 1)
        b = geq([(1,  1)], 1)

        b = b.copy()
        b = b.multiply(2)
        a = a.add(b)

        r = geq([], 1)
        assert r == a

    def test_add_canceling_4(self):
        a = geq([(2, -1), (1, 2)], 1)
        b = geq([(1,  -2), (1, 3)], 1)

        b = b.copy()
        b = b.multiply(2)
        a = a.add(b)

        r = geq([(2,-1), (1,-2), (2,3)], 2)
        assert r == a

    def test_add_canceling_5(self):
        a = geq([(2, -1), (1, 2), (2, -3)], 1)
        b = geq([(1,  -2), (1, 3)], 1)

        b = b.copy()
        b = b.multiply(2)
        a = a.add(b)

        r = geq([(2,-1), (1,-2)], 0)
        assert r == a

    def test_add_canceling_6(self):
        a = geq([(1, 1), (1, 2), (1, 3)], 1)
        b = geq([(1, 1), (1, -2)], 2)

        i = geq([], 0)
        a = a.copy()
        a = a.multiply(1)
        i = i.add(a)
        b = b.copy()
        b = b.multiply(1)
        i = i.add(b)

        r = geq([(2,1), (1,3)], 2)
        assert r == i

    def test_substitute_1(self):
        a = geq([(1, 2), (1, 1), (1, 3)], 2)
        b = geq([(1, 3)], 1)

        a = a.substitute([1, -2], [], [])
        assert(a == b)

    def test_substitute_2(self):
        a = geq([(1, 2), (1, 1), (1, 3)], 2)
        b = geq([(1, 3)], 1)

        a = a.substitute([-1, 2], [], [])
        assert(a == b)

    def test_substitute_3(self):
        a = geq([(1, 2), (1, 1), (1, 3)], 2)
        b = geq([(1, 3), (1,4)], 2)

        a = a.substitute([-1], [2], [4])
        assert(a == b)

    def test_substitute_3(self):
        a = geq([(1, 1), (1, 2), (1, 3)], 2)
        b = geq([(1, 3), (1,4)], 1)

        a = a.substitute([1], [2], [4])
        assert(a == b)

    def test_substitute_4(self):
        a = geq([(1, 1), (1, -2), (1, 3)], 2)
        b = geq([(1, 2), (1, -1), (1, 3)], 2)

        a = a.substitute([], [1,2], [2,1])
        assert(a == b)


    if not isinstance(ineqFactory, veripb.constraints.CppIneqFactory):
        def test_resolve_1(self):
            a = clause([1, 2])
            b = clause([-1, 3])
            r = a.resolve(b, 1)

            e = clause([2,3])

        def test_resolve_2(self):
            a = clause([1, 2])
            b = clause([3])
            r = a.resolve(b, 1)

            e = clause([3])

        def test_resolve_3(self):
            a = clause([2])
            b = clause([-1, 3])
            r = a.resolve(b, 1)

            e = clause([2])

        def test_resolve_fail_1(self):
            a = geq([(2,  1), (1, 2)], 1)
            b = geq([(1, -1), (1, 2)], 1)

            with self.assertRaises(NotImplementedError):
                r = a.resolve(b, 1)

        def test_resolve_fail_2(self):
            a = geq([(1,  1), (1, 2)], 1)
            b = geq([(1, -1), (1, 2)], 2)

            with self.assertRaises(NotImplementedError):
                r = a.resolve(b, 1)