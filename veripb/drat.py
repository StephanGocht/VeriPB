from veripb.rules import Rule, EmptyRule, ReverseUnitPropagation, LoadFormula
from veripb.parser import RuleParserBase, WordParser
from veripb.exceptions import ParseError, InvalidProof

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
            raise ParseError("Dublicate clauses are not supported.", column = 0)
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

class FindCheckFailed(InvalidProof):
    def __init__(self, expected):
        self.expected = expected

def parseIntList(words):
    lits = list()
    try:
        nxt = int(next(words))
        while (nxt != 0):
            lits.append(nxt)
            nxt = int(next(words))
    except StopIteration:
        raise ValueError("Expected 0 at end of constraint.")

    return lits

class DRATFind(EmptyRule):
    Id = "f"

    @classmethod
    def parse(cls, line, context):
        with WordParser(line) as words:
            lits = parseIntList(words)
        return cls(lits)

    def __init__(self, lits):
        self.lits = lits

    def compute(self, antecedents, context = None):
        clauseFinder = DRATClauseFinder.get(context)
        ClauseId = clauseFinder.find(self.lits)
        if ClauseId is None:
            raise FindCheckFailed(self.lits)

        return []


class DRATDeletion(EmptyRule):
    Id = "d"

    """
    Delete clauses based on DRAT input.

    For this to work it is crucial that no other rules than DRAT are
    used, otherwise the computed Id is not correct.
    """

    @classmethod
    def parse(cls, line, context):
        with WordParser(line) as words:
            lits = parseIntList(words)

        clauseFinder = DRATClauseFinder.get(context)
        ClauseId = clauseFinder.find(lits)

        return cls(ClauseId)

    def __init__(self, ClauseId):
        self.ClauseId = ClauseId

    def deleteConstraints(self):
        if self.ClauseId is not None:
            # print("delete", self.Id)
            return [self.ClauseId]
        else:
            return []


class DRAT(ReverseUnitPropagation):
    @classmethod
    def parse(cls, line, context):
        with WordParser(line) as words:
            lits = parseIntList(words)

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
        yield LoadFormula(len(self.context.formula))
        for x in super().parse([DRATDeletion, DRATFind], file, defaultRule = DRAT):
            yield x
