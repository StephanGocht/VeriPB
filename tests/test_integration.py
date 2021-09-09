import unittest

from pathlib import Path

from env import veripb
from veripb import run, InvalidProof, ParseError

from veripb.utils import Settings as MiscSettings
from veripb.verifier import Verifier

class TestIntegration(unittest.TestCase):
    def run_single(self, formulaPath):
        proofPath = formulaPath.with_suffix(".pbp")
        print("veripb %s %s"%(formulaPath, proofPath))

        miscSettings = MiscSettings({"arbitraryPrecision": True})
        verifierSettings = Verifier.Settings({"isCheckDeletionOn": True})

        with formulaPath.open() as formula:
            with proofPath.open() as proof:
                run(formula, proof, verifierSettings, miscSettings)

    def correct_proof(self, formulaPath):
        self.run_single(formulaPath)

    def incorrect_proof(self, formulaPath):
        try:
            self.run_single(formulaPath)
        except InvalidProof as e:
            pass
        else:
            self.fail("Proof should be invalid.")

    def parsing_failure(self, formulaPath):
        try:
            self.run_single(formulaPath)
        except ParseError as e:
            pass
        else:
            self.fail("Parsing should fail.")

def create(formulaPath, helper):
    def fun(self):
        helper(self, formulaPath)
    return fun

def findProblems(globExpression):
    current = Path(__file__).parent
    files = current.glob(globExpression)
    files = [f for f in files if f.suffix in [".cnf",".opb"] and f.is_file()]
    return files

correct = findProblems("integration_tests/correct/**/*.*")
for file in correct:
    method = create(file, TestIntegration.correct_proof)
    method.__name__ = "test_correct_%s"%(file.stem)
    setattr(TestIntegration, method.__name__, method)

incorrect = findProblems("integration_tests/incorrect/**/*.*")
for file in incorrect:
    method = create(file, TestIntegration.incorrect_proof)
    method.__name__ = "test_incorrect_%s"%(file.stem)
    setattr(TestIntegration, method.__name__, method)

parsing_failure = findProblems("integration_tests/parsing_failure/**/*.*")
for file in parsing_failure:
    method = create(file, TestIntegration.parsing_failure)
    method.__name__ = "test_fail_parsing_%s"%(file.stem)
    setattr(TestIntegration, method.__name__, method)



if __name__=="__main__":
    unittest.main()