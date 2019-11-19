import unittest
import io

from env import veripb
from veripb.pbsolver import Formula
from veripb.constraints import PyInequality, Term

class TestFormula(unittest.TestCase):
    def test_toString(self):
        f = Formula(
            [ PyInequality([Term(2,-1)], 1)
            , PyInequality([Term(2, 2)], 2)
            ]
        )

        stream = io.StringIO()
        f.write(stream)

        print(stream.getvalue())
        assert stream.getvalue() == \
            "* #variable= 42 #constraint= 2\n" \
            "+2 ~x1 >= 1;\n" \
            "+2 x2 >= 2;\n"
