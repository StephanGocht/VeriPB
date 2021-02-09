from veripb import InvalidProof
from veripb.rules import Rule, EmptyRule, register_rule
from veripb.rules import ReversePolishNotation, IsContradiction
from veripb.rules_register import register_rule, dom_friendly_rules, rules_to_dict
from veripb.parser import OPBParser, WordParser, ParseContext
from veripb.optimized.constraints import Substitution as CppSubstitution

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
        if len(self.substitutions) > 0:
            frm, to = zip(*self.substitutions)
        else:
            frm = []
            to = []

        return CppSubstitution(self.constants, frm, to)

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

class Autoprover():
    @TimedFunction.time("Autoprover::setup")
    def __init__(self, context, db, subgoals, upTo = None):
        self.context = context
        self.subgoals = subgoals
        self.upTo = upTo
        self.verbose = context.verifierSettings.trace
        self.context.propEngine.increaseNumVarsTo(context.ineqFactory.numVars())
        self.propEngine = context.propEngine

        self.dbSubstituted = None
        self.dbSet = None

    @TimedFunction.time("Autoprover::propagate")
    def propagate(self):
        self.assignment = Substitution()
        self.assignment.addConstants(self.propEngine.propagatedLits())
        #todo: do we want to check if the constraint is already rup?
        if self.verbose:
            print("    propagations:", end = " ")
            for i in self.assignment.constants:
                print(self.context.ineqFactory.int2lit(i), end = " ")
            print()

    @TimedFunction.time("Autoprover::selfImplication")
    def selfImplication(self, nxtGoalId, nxtGoal):
        if nxtGoalId in self.db:
            if self.db[nxtGoalId].implies(nxtGoal):
                if self.verbose:
                    print("    automatically proved %03i by self implication" % (nxtGoalId))
                return True
        return False

    @TimedFunction.time("Autoprover::inDB")
    def inDB(self, nxtGoalId, nxtGoal):
        if self.dbSet is None:
            self.dbSet = set(self.db.values())

        return nxtGoal in self.dbSet


    @TimedFunction.time("Autoprover::dbImplication")
    def dbImplication(self, nxtGoalId, nxtGoal):
        success = False
        if self.dbSubstituted is None:
            self.dbSubstituted = [(Id, ineq.copy().substitute(self.assignment.get())) for Id, ineq in self.db.items()]

        for ineqId, ineq in self.dbSubstituted:
            if ineq.implies(nxtGoal):
                success = True
                break

        if success:
            if self.verbose:
                print("    automatically proved %03i by implication from %i" % (nxtGoalId, ineqId))
            return True
        return False

    @TimedFunction.time("Autoprover::rupImplication")
    def rupImplication(self, nxtGoalId, nxtGoal):
        success = nxtGoal.ratCheck([], self.propEngine)
        if success:
            if self.verbose:
                print("    automatically proved %03i by RUP check" % (nxtGoalId))
            return True
        return False

    def __call__(self):
        self.propagate()

        sub = self.assignment.get()
        while self.subgoals:
            nxtGoalId, nxtGoal = self.subgoals[0]
            if self.upTo is not None and nxtGoalId >= self.upTo:
                break
            else:
                self.subgoals.popleft()

            nxtGoal = nxtGoal.substitute(sub)

            # if self.selfImplication(nxtGoalId, nxtGoal):
            #     continue

            # if self.inDB(nxtGoalId, nxtGoal):
            #     continue

            if self.rupImplication(nxtGoalId, nxtGoal):
                continue

            if self.dbImplication(nxtGoalId, nxtGoal):
                continue

            raise InvalidProof("Could not proof proof goal %03i automatically." % (nxtGoalId))


def autoProof(context, db, subgoals, upTo = None):
    if not subgoals:
        return

    prover = Autoprover(context, db, subgoals, upTo)
    prover()
