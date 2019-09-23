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
        parser = ConstraintEquals.getParser(self.context)
        rule = parser.parse("42 opb 3 x1 >= 2;")
        assert rule == ConstraintEquals(42, self.ineqFactory.fromTerms([Term(3,1)], 2))

    def test_implies(self):
        parser = ConstraintImplies.getParser(self.context)
        rule = parser.parse("42 opb 3 x1 >= 2;")
        assert rule == ConstraintImplies(42, self.ineqFactory.fromTerms([Term(3,1)], 2))

    def test_contradiction(self):
        parser = IsContradiction.getParser(self.context)
        rule = parser.parse("42 0")
        assert rule == IsContradiction(42)

    def test_linear_combination(self):
        parser = LinearCombination.getParser(self.context)
        rule = parser.parse(" 2 4 3 5 6 7 0")
        assert rule == LinearCombination((2,3,6), (4,5,7))

    def test_division(self):
        parser = Division.getParser(self.context)
        rule = parser.parse(" 3 2 4 3 5 6 7 0")
        assert rule == Division(3, (2,3,6), (4,5,7))

    def test_saturation(self):
        parser = Saturation.getParser(self.context)
        rule = parser.parse(" 2 4 3 5 6 7 0")
        assert rule == Saturation((2,3,6), (4,5,7))

class TestCheckConstraintParsing(unittest.TestCase):
    def setUp(self):
        self.ineqFactory = newDefaultFactory()
        self.context = DummyContext()
        self.context.ineqFactory = self.ineqFactory

    def test_1(self):
        parser = ConstraintEquals.getParser(self.context)
        res = parser.parse("1 opb 3 x1 >= 2;")
        assert res == ConstraintEquals(1, self.ineqFactory.fromTerms([Term(3,1)], 2))

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