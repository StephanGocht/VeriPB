from veripb import InvalidProof
from veripb.rules import Rule, EmptyRule, register_rule
from veripb.rules import ReversePolishNotation, IsContradiction
from veripb.rules_register import register_rule, dom_friendly_rules, rules_to_dict
from veripb.parser import OPBParser, WordParser, ParseContext

from veripb import verifier

from veripb.timed_function import TimedFunction

from collections import deque

class Substitution:
    def __init__(self):
        self.constants = []
        self.substitutions = []
        self.isSorted = True

    def mapAll(self, variables, substitutes):
        for variable, substitute in zip(variables, substitutes):
            self.map(variable, substitute)

    def addConstants(self, constants):
        self.constants.extend(constants)

    def map(self, variable, substitute):
        self.isSorted = False

        if variable < 0:
            raise ValueError("Substitution should only map variables.")

        if substitute is False:
            self.constants.append(-variable)
        elif substitute is True:
            self.constants.append(variable)
        else:
            self.substitutions.append((variable,substitute))

    def get(self):
        if not self.isSorted:
            self.sort()

        if len(self.substitutions) > 0:
            frm, to = zip(*self.substitutions)
        else:
            frm = []
            to = []

        return (self.constants, frm, to)

    def asDict(self):
        res = dict()
        for var, value in self.substitutions:
            res[var] = value
            res[-var] = -value

        for lit in self.constants:
            res[lit] = True
            res[-lit] = False

        return res

    @classmethod
    def fromDict(cls, sub):
        res = Substitution()
        for key, value in sub.items():
            res.map(key, value)
        res.sort()
        return res

    def sort(self):
        self.substitutions.sort(key = lambda x: x[0])
        self.isSorted = True

    @classmethod
    def parse(cls, words, ineqFactory, forbidden = []):
        result = cls()

        try:
            nxt = next(words)
        except StopIteration:
            raise ValueError("Expected substitution found nothing.")

        while nxt != ";":
            frm = ineqFactory.lit2int(nxt)
            if frm < 0:
                raise ValueError("Substitution should only"
                    "map variables, not negated literals.")
            if frm in forbidden:
                raise ValueError("Substitution contains forbidden variable.")

            try:
                nxt = next(words)
                if nxt == "→" or nxt == "->":
                    nxt = next(words)
            except StopIteration:
                raise ValueError("Substitution is missing"
                    "a value for the last variable.")

            if nxt == "0":
                to = False
            elif nxt == "1":
                to = True
            else:
                to  = ineqFactory.lit2int(nxt)

            result.map(frm,to)

            try:
                nxt = next(words)

                if nxt == ",":
                    nxt = next(words)
            except StopIteration:
                break

        result.sort()
        return result


@TimedFunction.time("AutoProoving")
def autoProof(context, db, subgoals, upTo = None):
    if not subgoals:
        return

    verbose = context.verifierSettings.trace

    context.propEngine.increaseNumVarsTo(context.ineqFactory.numVars())
    propEngine = context.propEngine

    assignment = Substitution()
    assignment.addConstants(propEngine.propagatedLits())
    if verbose:
        print("    propagations:", end = " ")
        for i in assignment.constants:
            print(context.ineqFactory.int2lit(i), end = " ")
        print()

    db = {Id: ineq for Id, ineq in db}
    dbSubstituted = None

    while subgoals:
        nxtGoalId, nxtGoal = subgoals[0]
        if upTo is not None and nxtGoalId >= upTo:
            break
        else:
            subgoals.popleft()

        nxtGoal = nxtGoal.substitute(*assignment.get())

        if nxtGoalId in db:
            if db[nxtGoalId].implies(nxtGoal):
                if verbose:
                    print("    automatically proved %03i by self implication" % (nxtGoalId))
                continue

        success = nxtGoal.ratCheck([], propEngine)
        if success:
            if verbose:
                print("    automatically proved %03i by RUP check" % (nxtGoalId))
            continue

        if dbSubstituted is None:
            dbSubstituted = [(Id, ineq.copy().substitute(*assignment.get())) for Id, ineq in db.items()]

        for ineqId, ineq in dbSubstituted:
            if ineq.implies(nxtGoal):
                success = True
                break

        if success:
            if verbose:
                print("    automatically proved %03i by implication from %i" % (nxtGoalId, ineqId))
            continue

        raise InvalidProof("Could not proof proof goal %03i automatically." % (nxtGoalId))
