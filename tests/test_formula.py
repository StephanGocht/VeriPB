import unittest
import io

from env import refpy
from refpy.pbsolver import Formula
from refpy.constraints import Inequality, Term

class TestInequality(unittest.TestCase):
    def test_toString(self):
        f = Formula(
            [ Inequality([Term(2,-1)], 1)
            , Inequality([Term(2, 2)], 2)
            ]
        )

        stream = io.StringIO()
        f.write(stream)

        print(stream.getvalue())
        assert stream.getvalue() == \
            "* #variable= 2 #constraint= 2\n" \
            "+2 ~x1 >= 1;\n" \
            "+2 x2 >= 2;\n"
