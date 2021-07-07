import unittest

from env import veripb
from veripb.parser import *
from veripb.rules import *
from veripb.rules_register import get_registered_rules
from veripb.constraints import Term
from veripb.constraints import CppIneqFactory as IneqFactory


class DummyContext:
    pass

def lit2str(lit):
    if lit < 0:
        return "~x%i"%(-lit)
    else:
        return "x%i"%(lit)

class TestParsing(unittest.TestCase):

    def checkParsing(self, terms, degree, eq = False):
        termString = " ".join(["%i %s" % (coeff, lit2str(lit)) for coeff, lit in terms])
        op = "=" if eq else ">="
        string = "%s %s %i ;" % (termString, op, degree)
        terms = [Term(coeff, lit) for coeff, lit in terms]
        expect = []
        expect.append(self.ineqFactory.fromTerms(terms, degree))
        if eq:
            for term in terms:
                term.coefficient = -term.coefficient
            degree = -degree
            expect.append(self.ineqFactory.fromTerms(terms, degree))

        res = self.ineqFactory.parseString(string, allowMultiple = True)
        assert res == expect

    def setUp(self):
        self.ineqFactory = IneqFactory(False)
        self.context = DummyContext()
        self.context.ineqFactory = self.ineqFactory

    def test_header(self):
        parser = RuleParser(DummyContext())
        print(parser.parse(get_registered_rules(), ["pseudo-Boolean proof version 1.0"]))

    def test_OPB_line_1(self):
        res = self.ineqFactory.parseString("3 x1 >= 2 ;", allowMultiple = True)
        assert res == [self.ineqFactory.fromTerms([Term(3,1)], 2)]

    def test_OPB_line_2(self):
        res = OPBParser(self.ineqFactory).parseOPB("+2 x1 -4 x2 = -42;".split())
        assert res == [
            self.ineqFactory.fromTerms([Term(2,1), Term(-4,2)], -42),
            self.ineqFactory.fromTerms([Term(-2,1), Term(4,2)], 42)]

    def test_eq(self):
        res = OPBParser(self.ineqFactory).parseOPB("1 x2 -2 x1 = 2;".split())

        if self.ineqFactory.freeNames:
            ineq1 = self.ineqFactory.fromTerms([Term(-2,2), Term(1,1)], 2)
            ineq2 = self.ineqFactory.fromTerms([Term(2,2), Term(-1,1)], -2)
        else:
            ineq1 = self.ineqFactory.fromTerms([Term(-2,1), Term(1,2)], 2)
            ineq2 = self.ineqFactory.fromTerms([Term(2,1), Term(-1,2)], -2)

        assert res == [ineq1, ineq2]

    def test_CNF_line(self):
        res = OPBParser(self.ineqFactory).parseCNF("-1 2 0".split())
        assert res == [self.ineqFactory.fromTerms([Term(1,2), Term(1,-1)], 1)]

    def test_equals(self):
        rule = ConstraintEquals.parse("42 3 x1 >= 2;", self.context)
        assert rule == ConstraintEquals(42, self.ineqFactory.fromTerms([Term(3,1)], 2))

    def test_implies(self):
        rule = ConstraintImplies.parse("42 3 x1 >= 2;", self.context)
        assert rule == ConstraintImplies(42, self.ineqFactory.fromTerms([Term(3,1)], 2))

    def test_contradiction(self):
        rule = IsContradiction.parse("42 0", self.context)
        assert rule == IsContradiction(42)

    def test_clause_1(self):
        self.checkParsing([(1, 1)], 1)
        self.checkParsing([(1, 1)], 1, eq = True)

    def test_clause_2(self):
        self.checkParsing([(1, 1), (1, 2)], 1)
        self.checkParsing([(1, 1), (1, 2)], 1, eq = True)

    def test_clause_3(self):
        self.checkParsing([(1, 1), (1, -2) ], 1)
        self.checkParsing([(1, 1), (1, -2) ], 1, eq = True)

    def test_clause_4(self):
        self.checkParsing([(1, 1), (-1, -2)], 0)
        self.checkParsing([(1, 1), (-1, -2)], 0, eq = True)

    def test_ineq_1(self):
        val = 214748364
        self.checkParsing([(val, 1)], val)
        self.checkParsing([(val, 1)], val, eq = True)

    def test_ineq_1(self):
        val = 214748364
        self.checkParsing([(-val, 1)], val)
        self.checkParsing([(-val, 1)], val, eq = True)

    def test_ineq_1(self):
        val = 214748364
        self.checkParsing([(-val, -1)], val)
        self.checkParsing([(-val, -1)], val, eq = True)

    def test_int32_max_almost(self):
        val = 2147483647 - 1
        self.checkParsing([(val, 1)], val)
        self.checkParsing([(val, 1)], val, eq = True)

    def test_int32_max(self):
        val = 2147483647
        self.checkParsing([(val, 1)], val)
        self.checkParsing([(val, 1)], val, eq = True)

    def test_int32_max_more(self):
        val = 2147483647 + 1
        self.checkParsing([(val, 1)], val)
        self.checkParsing([(val, 1)], val, eq = True)

    def test_small_large_coeff(self):
        val = 2147483647 + 1
        self.checkParsing([(1, 2), (val, 1)], val)
        self.checkParsing([(1, 2), (val, 1)], val, eq = True)

    def test_small_large_degree(self):
        val = 2147483647 + 1
        self.checkParsing([(1, 2), (1, 1)], val)
        self.checkParsing([(1, 2), (1, 1)], val, eq = True)

    def test_degree_overflow(self):
        val = 2147483647
        self.checkParsing([(-val//2, 1), (-val//2, 2), (-val//2, 3), (-val//2, 4)], 0)
        self.checkParsing([(-val//2, 1), (-val//2, 2), (-val//2, 3), (-val//2, 4)], 0, eq = True)


class TestWordParser(unittest.TestCase):
    def test_working_1(self):
        line = "this is a test sentence"
        with WordParser(line) as words:
            for word in words:
                pass

    def test_working_2(self):
        line = "fancy 1 2 3 4"
        with WordParser(line) as words:
            it = iter(words)
            assert(next(it) == "fancy")
            nums = list(map(int, it))
            assert(nums == [1,2,3,4])

    def test_exception_int(self):
        try:
            line = "  fancy 1 2  a 4"
            with WordParser(line) as words:
                it = iter(words)
                assert(next(it) == "fancy")
                nums = list(map(int, it))
        except ParseError as e:
            assert(e.column == 14)
        else:
            assert(False)


    def test_exception_manual(self):
        try:
            line = "  this  notis a test sentence"
            with WordParser(line) as words:
                it = iter(words)
                assert(next(it) == "this")
                if next(it) != "is":
                    raise ValueError("Expected is.")
        except ParseError as e:
            assert(e.column == 9)
        else:
            assert(False)
