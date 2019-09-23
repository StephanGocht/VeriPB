import unittest

from env import refpy
from refpy.parser import *
from refpy.rules import *
from refpy.constraints import Term
from refpy.constraints import newDefaultFactory


class DummyContext:
    pass

class TestParsing(unittest.TestCase):
    def setUp(self):
        self.ineqFactory = newDefaultFactory()
        self.context = DummyContext()
        self.context.ineqFactory = self.ineqFactory

    def test_header(self):
        parser = RuleParser(DummyContext())
        print(parser.parse(registered_rules, ["refutation using e 0"]))

    def test_OPB_line_1(self):
        res = OPBParser(self.ineqFactory).getOPBConstraintParser().parse("3 x1 >= 2;")
        assert res == ([(3,1)], ">=", 2)

    def test_OPB_line_2(self):
        res = OPBParser(self.ineqFactory).getOPBConstraintParser().parse("-4 x2 +2 x1 = -42;")
        assert res == ([(-4,1),(2,2)], "=", -42)

    def test_CNF_line(self):
        res = OPBParser(self.ineqFactory).getCNFConstraintParser().parse("2 -1 0")
        assert res == [2, -1]

    def test_equals(self):
        rule = ConstraintEquals.parse("42 opb 3 x1 >= 2;", self.context)
        assert rule == ConstraintEquals(42, self.ineqFactory.fromTerms([Term(3,1)], 2))

    def test_implies(self):
        rule = ConstraintImplies.parse("42 opb 3 x1 >= 2;", self.context)
        assert rule == ConstraintImplies(42, self.ineqFactory.fromTerms([Term(3,1)], 2))

    def test_contradiction(self):
        rule = IsContradiction.parse("42 0", self.context)
        assert rule == IsContradiction(42)

class TestInequalityParsing(unittest.TestCase):
    def setUp(self):
        self.ineqFactory = newDefaultFactory()

    def test_eq(self):
        parser = OPBParser(self.ineqFactory).getOPBParser()
        result = parser.parse("1 x2 -2 x1 = 2;")

        ineq1 = self.ineqFactory.fromTerms([Term(-2,2), Term(1,1)], 2)
        ineq2 = self.ineqFactory.fromTerms([Term(2,2), Term(-1,1)], -2)

        assert len(result) == 2
        print(result)
        assert (result[0] == ineq1 and result[1] == ineq2) \
            or (result[0] == ineq2 and result[1] == ineq1)

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
