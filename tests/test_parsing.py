import unittest

from env import refpy
from refpy.parser import *
from refpy.rules import *
from refpy.constraints import Term
from refpy.constraints import defaultFactory as ineqFactory

class TestParsing():
    def test_header(self):
        parser = RuleParser()
        print(parser.parse(registered_rules, ["refutation using e 0"]))

    def test_OPB_line_1(self):
        res = getOPBConstraintParser().parse("3 x1 >= 2;")
        assert res == ([(3,1)], ">=", 2)

    def test_OPB_line_2(self):
        res = getOPBConstraintParser().parse("-4 x2 +2 x1 = -42;")
        assert res == ([(-4,2),(2,1)], "=", -42)

    def test_CNF_line(self):
        res = getCNFConstraintParser().parse("2 -1 0")
        assert res == [2, -1]

    def test_equals(self):
        parser = ConstraintEquals.getParser()
        rule = parser.parse("42 opb 3 x1 >= 2;")
        assert rule == ConstraintEquals(42, ineqFactory.fromTerms([Term(3,1)], 2))

    def test_implies(self):
        parser = ConstraintImplies.getParser()
        rule = parser.parse("42 opb 3 x1 >= 2;")
        assert rule == ConstraintImplies(42, ineqFactory.fromTerms([Term(3,1)], 2))

    def test_contradiction(self):
        parser = IsContradiction.getParser()
        rule = parser.parse("42 0")
        assert rule == IsContradiction(42)

    def test_linear_combination(self):
        parser = LinearCombination.getParser()
        rule = parser.parse(" 2 4 3 5 6 7 0")
        assert rule == LinearCombination((2,3,6), (4,5,7))

    def test_division(self):
        parser = Division.getParser()
        rule = parser.parse(" 3 2 4 3 5 6 7 0")
        assert rule == Division(3, (2,3,6), (4,5,7))

    def test_saturation(self):
        parser = Saturation.getParser()
        rule = parser.parse(" 2 4 3 5 6 7 0")
        assert rule == Saturation((2,3,6), (4,5,7))

class TestCheckConstraintParsing():
    def test_1(self):
        parser = ConstraintEquals.getParser()
        res = parser.parse("1 opb 3 x1 >= 2;")
        assert res == ConstraintEquals(1, ineqFactory.fromTerms([Term(3,1)], 2))

class TestInequalityParsing():
    def test_eq(self):
        parser = ineqFactory.getOPBParser()
        result = parser.parse("1 x2 -2 x1 = 2;")

        ineq1 = ineqFactory.fromTerms([Term(-2,1), Term(1,2)], 2)
        ineq2 = ineqFactory.fromTerms([Term(2,1), Term(-1,2)], -2)

        assert len(result) == 2
        print(result)
        assert (result[0] == ineq1 and result[1] == ineq2) \
            or (result[0] == ineq2 and result[1] == ineq1)