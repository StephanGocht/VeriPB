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
        parser = CheckConstraint.getParser()
        res = parser.parse("1 opb 3 x1 >= 2;")
        assert res == CheckConstraint(1, ([(3,1)], ">=", 2))