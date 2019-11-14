from refpy.rules import Rule, EmptyRule, ReverseUnitPropagation, LoadFormula
from refpy.parser import RuleParserBase, WordParser

class DRATClauseFinder():
    @classmethod
    def get(cls, context):
        try:
            return context.dratClauseFinder
        except AttributeError:
            context.dratClauseFinder = DRATClauseFinder(len(context.formula) + 1)
            return context.dratClauseFinder

    def __init__(self, firstId):
        self.clauses = dict()
        self.maxId = firstId

    def add(self, lits):
        clause = tuple(sorted(lits))
        if clause in self.clauses:
            raise ValueError("Doublicate clauses are not supported.")
        self.clauses[clause] = self.maxId
        self.maxId += 1

    def find(self, lits):
        clause = tuple(sorted(lits))
        Id = self.clauses.get(clause, None)
        if Id is None:
            pass
            # todo: solvers might erase clauses from the formula, which is currently not supported.
            # raise ValueError("Erasing clause that was never added.")
        else:
            del self.clauses[clause]
        return Id

class DRATDeletion(EmptyRule):
    Id = "d"

    """
    Delete clauses based on DRAT input.

    For this to work it is crucial that no other rules than DRAT are
    used, otherwise the computed Id is not correct.
    """

    @classmethod
    def parse(cls, line, context):
        lits = list()
        with WordParser(line) as words:
            try:
                nxt = int(next(words))
                while (nxt != 0):
                    lits.append(nxt)
                    nxt = int(next(words))
            except StopIteration:
                raise ValueError("Expected 0 at end of constraint.")

        clauseFinder = DRATClauseFinder.get(context)
        Id = clauseFinder.find(lits)

        return cls(Id)

    def __init__(self, Id):
        self.Id = Id

    def deleteConstraints(self):
        if self.Id is not None:
            # print("delete", self.Id)
            return [self.Id]
        else:
            return []


class DRAT(ReverseUnitPropagation):
    @classmethod
    def parse(cls, line, context):
        lits = list()
        with WordParser(line) as words:
            for w in words:
                if w == "0":
                    break
                else:
                    lits.append(int(w))

        clauseFinder = DRATClauseFinder.get(context)
        clauseFinder.add(lits)

        terms = [(1,context.ineqFactory.intlit2int(l)) for l in lits]
        ineq = context.ineqFactory.fromTerms(terms, 1)
        if len(lits) > 0:
            w = [terms[0][1]]
        else:
            w = []
        return cls(ineq, w, context.ineqFactory.numVars())


class DRATParser(RuleParserBase):
    commentChar = "c"

    def parse(self, file):
        return [LoadFormula(len(self.context.formula))] + super().parse([DRATDeletion], file, defaultRule = DRAT)
