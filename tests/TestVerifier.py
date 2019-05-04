import unittest
import env

from main import Verifier, Rule

class MockRule(Rule):
    def __init__(self, Id, numConstraints, antecedents = [], isGoal = False):
        self._id = Id
        self._numConstraints = numConstraints
        self._antecedents = antecedents
        self._isGoal = isGoal

    def computeConstraints(self, antecedents):
        return [(self._id, i) for i in range(self._numConstraints)]

    def numConstraints(self):
        return self._numConstraints

    def antecedentIDs(self):
        return self._antecedents

    def isGoal(self):
        return self._isGoal

class TestVerifierImpl():
    def test_empty(self):
        verify = Verifier();
        verify([])

    def test_findGoal_single(self):
        verify = Verifier();
        verify.clean()

        proof = [
            MockRule(1, 1, isGoal = True)
        ]
        verify.forwardScan(proof)
        assert verify.db[1].rule == proof[0]
        assert len(verify.goals) == 1

        verify.markUsed()
        verify.compute()

        assert verify.db[1].constraint == (1,0)


    def test_findGoal_multi(self):
        verify = Verifier();
        verify.clean()
        verify.forwardScan([
            MockRule(1, 2, isGoal = False),
            MockRule("g1", 1, isGoal = True),
            MockRule("g2", 3, isGoal = True)
        ])
        assert len(verify.goals) == 4

        verify.markUsed()
        verify.compute()

        assert verify.db[3].constraint == ("g1",0)
        assert verify.db[4].constraint == ("g2",0)
        assert verify.db[5].constraint == ("g2",1)
        assert verify.db[6].constraint == ("g2",2)
