import unittest

from env import veripb
from veripb.parser import *
from veripb.rules import *
from veripb.constraints import Term
from veripb.constraints import CppIneqFactory as IneqFactory


class DummyContext:
    pass

class TestParsing(unittest.TestCase):
    def setUp(self):
        self.ineqFactory = IneqFactory()
        self.context = DummyContext()
        self.context.ineqFactory = self.ineqFactory

    def test_header(self):
        parser = RuleParser(DummyContext())
        print(parser.parse(registered_rules, ["pseudo-Boolean proof version 1.0"]))

    def test_OPB_line_1(self):
        res = OPBParser(self.ineqFactory).parseOPB("3 x1 >= 2;".split())
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
