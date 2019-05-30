import unittest


from env import refpy
from refpy.verifier import Verifier
from refpy.rules import *
from refpy import InvalidProof
from unittest.mock import MagicMock

class MockRule(Rule):
    def __init__(self, Id, numConstraints, antecedentIDs = [], isGoal = False):
        self._id = Id
        self._numConstraints = numConstraints
        self._antecedentIDs = antecedentIDs
        self._isGoal = isGoal

    def compute(self, antecedents):
        self._antecedents = antecedents
        return [(self._id, i) for i in range(self._numConstraints)]

    def numConstraints(self):
        return self._numConstraints

    def antecedentIDs(self):
        return self._antecedentIDs

    def isGoal(self):
        return self._isGoal

class TestVerifierImpl(unittest.TestCase):
    def test_empty(self):
        verify = Verifier();
        verify([])

    def test_findGoal_single_self(self):
        settings = Verifier.Settings()
        settings.checkInvariants = True
        settings.disableDeletion = True
        settings.skipUnused      = False

        verify = Verifier(settings)

        proof = [
            MockRule(1, 1, isGoal = True)
        ]

        verify.init(proof)
        verify.mapRulesToDB()
        assert verify.db[1].rule == proof[0]
        assert len(verify.goals) == 1

        verify.markUsed()
        verify.compute()

        assert verify.db[1].constraint == (1,0)

    def test_findGoal_single_other(self):
        settings = Verifier.Settings()
        settings.checkInvariants = True
        settings.disableDeletion = True
        settings.skipUnused      = False

        verify = Verifier(settings)

        proof = [
            MockRule(1, 1, isGoal = False),
            MockRule(1, 0, [1], isGoal = True)
        ]
        verify.init(proof)
        verify.mapRulesToDB()
        assert verify.db[1].rule == proof[0]
        assert len(verify.goals) == 1

        verify.markUsed()
        verify.compute()

        assert verify.db[1].constraint == (1,0)


    def findGoal_multi(self, option, result):
        settings = Verifier.Settings()
        settings.checkInvariants = True
        settings.setPreset(option)

        verify = Verifier(settings)

        proof = [
            MockRule("r1", 2, isGoal = False),
            MockRule("g1", 1, isGoal = True),
            MockRule("g2", 2, isGoal = True),
        ]

        verify.init(proof)
        verify.mapRulesToDB()

        expectedDB = ["r1", "r1", "g1", "g2", "g2"]
        assert expectedDB == [line.rule._id for line in verify.db[1:]]

        assert len(verify.goals) == 3

        verify.markUsed()

        verify.compute()

        assert [c.constraint for c in verify.db[1:]] == result


    def test_findGoal_multi1(self):
        option = {
            "disableDeletion" : True,
            "skipUnused" : False
        }
        result = [
            ("r1", 0),
            ("r1", 1),
            ("g1", 0),
            ("g2", 0),
            ("g2", 1)
        ]
        self.findGoal_multi(option, result)

    def test_findGoal_multi2(self):
        option = {
            "disableDeletion" : True,
            "skipUnused" : True
        }
        result = [
            None, # unused constraint
            None, # unused constraint
            ("g1", 0),
            ("g2", 0),
            ("g2", 1)
        ]
        self.findGoal_multi(option, result)

    def test_findGoal_multi3(self):
        option = {
            "disableDeletion" : False,
            "skipUnused" : True
        }
        result = [
            None,
            None,
            None,
            None,
            None
        ]
        self.findGoal_multi(option, result)
