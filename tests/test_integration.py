import unittest

from pathlib import Path

import env
from main import run, InvalidProof

class TestIntegration(unittest.TestCase):
    def run_single(self, formulaPath):
        proofPath = formulaPath.with_suffix(".proof")
        with formulaPath.open() as formula:
            with proofPath.open() as proof:
                run(formula, proof)

    def correct_proof(self, formulaPath):
        self.run_single(formulaPath)

    def incorrect_proof(self, formulaPath):
        try:
            self.run_single(formulaPath)
        except InvalidProof as e:
            pass
        else:
            self.fail("Proof should be invalid.")



def create(formulaPath, correct):
    def fun(self):
        if correct:
            self.correct_proof(formulaPath)
        else:
            self.incorrect_proof(formulaPath)

    return fun

current = Path(__file__).parent
correct = current.glob("integration_tests/correct/**/*.opb")
incorrect = current.glob("integration_tests/incorrect/**/*.opb")

for file in correct:
    method = create(file, correct = True)
    method.__name__ = "test_correct_%s"%(file.stem)
    setattr(TestIntegration, method.__name__, method)

for file in incorrect:
    method = create(file, correct = False)
    method.__name__ = "test_incorrect_%s"%(file.stem)
    setattr(TestIntegration, method.__name__, method)

if __name__=="__main__":
    unittest.main()