import env

import unittest

from main import *

class TestParsing():
    def test_header(self):
        parser = RuleParser()
        print(parser.parse(registered_rules, "refutation graph using e 0"))

    def test_OPB_line_1(self):
        res = getOPBConstraintParser().parse("3x1 >= 2;")
        assert res == ([(3,1)], ">=", 2)

    def test_OPB_line_2(self):
        res = getOPBConstraintParser().parse("-4x2 +2x1 = -42;")
        assert res == ([(-4,2),(2,1)], "=", -42)

    def test_CNF_line(self):
        res = getCNFConstraintParser().parse("2 -1 0")
        assert res == [2, -1]

    def test_equals(self):
        parser = RuleParser()
        print(registered_rules[0].Id)
        rules = parser.parse(registered_rules, "refutation graph using e 0\ne 1 opb 3 x1 >= 2;")
        print(rules)

class TestCheckConstraintParsing():
    def test_1(self):
        parser = ConstraintEquals.getParser()
        res = parser.parse("1 opb 3 x1 >= 2;")
        assert res == ConstraintEquals(1, Inequality([Term(3,1)], 2))

class TestInequalityParsing():
    def test_eq(self):
        parser = Inequality.getOPBParser()
        result = parser.parse("1x2 -2x1 = 2;")

        ineq1 = Inequality([Term(-2,1), Term(1,2)], 2)
        ineq2 = Inequality([Term(2,1), Term(-1,2)], -2)

        assert len(result) == 2
        print(result)
        assert (result[0] == ineq1 and result[1] == ineq2) \
            or (result[0] == ineq2 and result[1] == ineq1)