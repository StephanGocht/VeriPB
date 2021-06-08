from veripb.rules import Rule, EmptyRule, ReverseUnitPropagation, LoadFormula
from veripb.parser import RuleParserBase, MaybeWordParser
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

    def normalized(self, lits):
        return tuple(sorted(lits))

    def increase(self, lits):
        """add one entry, returns true if no entries was there before"""

        clause = self.normalized(lits)
        data = self.clauses.setdefault(clause, {"id": self.maxId, "count": 0})
        if data["count"] == 0:
            self.maxId += 1
        data["count"] += 1

        return data["count"] == 1

    def decrease(self, lits):
        """remove one entry, returns entrie if no entries are left"""
        clause = self.normalized(lits)
        data = self.clauses.get(clause, {"id": 0, "count": 0})
        data["count"] -= 1
        if (data["count"] == 0):
            del self.clauses[clause]

        # todo: add support for deleting / unattaching formula
        # constraints
        return None if data["count"] != 0 else data["id"]

    def find(self, lits):
        clause = self.normalized(lits)
        return clause in self.clauses

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
    Ids = ["f"]

    @classmethod
    def parse(cls, line, context):
        with MaybeWordParser(line) as words:
            lits = parseIntList(words)
        return cls(lits)

    def __init__(self, lits):
        self.lits = lits

    def compute(self, antecedents, context = None):
        clauseFinder = DRATClauseFinder.get(context)
        if not clauseFinder.find(self.lits):
            raise FindCheckFailed(self.lits)

        return []


class DRATDeletion(EmptyRule):
    Ids = ["d"]

    """
    Delete clauses based on DRAT input.

    For this to work it is crucial that no other rules than DRAT are
    used, otherwise the computed Id is not correct.
    """

    @classmethod
    def parse(cls, line, context):
        with MaybeWordParser(line) as words:
            lits = parseIntList(words)

        clauseFinder = DRATClauseFinder.get(context)
        ClauseId = clauseFinder.decrease(lits)

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
        with MaybeWordParser(line) as words:
            lits = parseIntList(words)

            clauseFinder = DRATClauseFinder.get(context)
            isNew = clauseFinder.increase(lits)

            if isNew:
                terms = [(1,context.ineqFactory.intlit2int(l)) for l in lits]
                ineq = context.ineqFactory.fromTerms(terms, 1)
                if len(lits) > 0:
                    w = [terms[0][1]]
                else:
                    w = []
                isContradiction = (len(lits) == 0)
                return cls(ineq, w, context.ineqFactory.numVars(), isContradiction)
            else:
                return EmptyRule()

    def __init__(self, ineq, w, numVars, isContradiction):
        super().__init__(ineq, w)
        self.isContradiction = isContradiction

    def compute(self, antecedents, context):
        if self.isContradiction:
            context.containsContradiction = True

        return super().compute(antecedents, context)



class DRATParser(RuleParserBase):
    commentChar = "c"

    def parse(self, file):
        yield LoadFormula(len(self.parseContext.context.formula))
        for x in super().parse([DRATDeletion, DRATFind], file, defaultRule = DRAT):
            yield x
