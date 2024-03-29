from veripb import InvalidProof
from veripb.rules import Rule, EmptyRule, register_rule
from veripb.rules import ReversePolishNotation, IsContradiction
from veripb.rules_register import register_rule, dom_friendly_rules, rules_to_dict
from veripb.parser import OPBParser, MaybeWordParser, ParseContext
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
        self.isSorted = False
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
        if not self.isSorted:
            self.substitutions.sort(key = lambda x: x[0])
            self.constants.sort()
            self.isSorted = True

    @classmethod
    def parse(cls, words, ineqFactory, forbidden = []):
        result = cls()

        try:
            nxt = next(words)
        except StopIteration:
            return result

        while nxt != ";":
            frm = ineqFactory.lit2int(nxt)
            if frm < 0:
                raise ValueError("Substitution should only"
                    "map variables, not negated literals.")
            if frm in forbidden:
                raise InvalidProof("Substitution contains forbidden variable.")

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

        return result

    def __eq__(self, other):
        self.sort()
        return self.constants == other.constants \
            and self.substitutions == other.substitutions

    def __hash__(self):
        self.sort()
        return hash((tuple(self.constants), tuple(self.substitutions)))

class TemporaryAttach():
    def __init__(self, propEngine):
        self.propEngine = propEngine
        self.attached = []

    def attach(self, constraint):
        self.attached.append(self.propEngine.attach(constraint, 0))


    def detachAll(self):
        for c in self.attached:
            self.propEngine.detach(c, 0)

    def __enter__(self):
        return self

    def __exit__(self, exec_type, exec_value, exec_traceback):
        self.detachAll()

class Autoprover():
    #@TimedFunction.time("Autoprover::setup")
    def __init__(self, context, db, subgoals):
        self.context = context
        self.subgoals = subgoals
        self.verbose = context.verifierSettings.trace
        self.context.propEngine.increaseNumVarsTo(context.ineqFactory.numVars())
        self.propEngine = context.propEngine
        self.db = db
        self.dbSubstituted = None
        self.dbSet = None
        self.assignment = None
        self.wasRUP = False
        self.triedRUP = False

    # @TimedFunction.time("Autoprover::propagate")
    def propagate(self):
        self.assignment = Substitution()
        self.assignment.addConstants(self.propEngine.propagatedLits())
        #todo: do we want to check if the constraint is already rup?
        if self.verbose:
            print("    propagations:", end = " ")
            for i in self.assignment.constants:
                print(self.context.ineqFactory.int2lit(i), end = " ")
            print()

    #@TimedFunction.time("Autoprover::selfImplication")
    def selfImplication(self, nxtGoalId, nxtGoal):
        if nxtGoalId in self.db:
            if self.db[nxtGoalId].implies(nxtGoal):
                if self.verbose:
                    print("    automatically proved %s by self implication" % (str(nxtGoalId)))
                return True
        return False

    def inDB(self, nxtGoalId, nxtGoal):
        success = self.propEngine.find(nxtGoal)
        if success:
            if self.verbose:
                print("    automatically proved %s by finding constraint in database" % (str(nxtGoalId)))
            return True
        return False

    #@TimedFunction.time("Autoprover::dbImplication")
    def dbImplication(self, nxtGoalId, nxtGoal):
        success = False
        if self.dbSubstituted is None:
            asmnt = self.getPropagatedAssignment().get()
            self.dbSubstituted = [(Id, ineq.copy().substitute(asmnt)) for Id, ineq in self.db]

        for ineqId, ineq in self.dbSubstituted:
            if ineq.implies(nxtGoal):
                success = True
                break

        if success:
            if self.verbose:
                print("    automatically proved %s by implication from %i" % (str(nxtGoalId), ineqId))
            return True
        return False

    #@TimedFunction.time("Autoprover::rupImplication")
    def rupImplication(self, nxtGoalId, nxtGoal):
        success = nxtGoal.rupCheck(self.propEngine, False)
        if success:
            if self.verbose:
                if nxtGoalId is not None:
                    print("    automatically proved %s by RUP check." % (str(nxtGoalId)))
                else:
                    print("    automatically proved constraint to be added by RUP check.")
                    self.getPropagatedAssignment()
            return True
        return False


    def getPropagatedAssignment(self):
        if self.assignment is None:
            self.propagate()

        return self.assignment

    @TimedFunction.time("Autoprover")
    def __call__(self):
        while self.subgoals:
            nxtGoalId, nxtGoal = self.subgoals.popleft()

            ## for performance reasons the following two checks are
            ## done directly when the effected constraints are computed
            #
            # if self.selfImplication(nxtGoalId, nxtGoal):
            #     continue

            asRhs = nxtGoal.getAsRightHand()
            if asRhs is None:
                asLhs = nxtGoal.getAsLeftHand()

                with TemporaryAttach(self.propEngine) as temporary:
                    for c in asLhs:
                        if c is not None:
                            temporary.attach(c)

                    if self.rupImplication(nxtGoalId, self.context.ineqFactory.fromTerms([], 1)):
                        continue

            else:
                nxtGoal = asRhs.copy()
                if nxtGoal.isTrivial():
                    if self.verbose:
                        print("    automatically proved %s, constraint is trivial." % (str(nxtGoalId)))
                    continue

                if not self.triedRUP and self.rupImplication(None, self.context.ineqFactory.fromTerms([], 1)):
                    self.wasRUP = True
                    break

                self.triedRUP = True

                if self.rupImplication(nxtGoalId, nxtGoal):
                    continue

                # implication checks are stronger if we plug in propagated literals
                asmnt = self.getPropagatedAssignment().get()
                nxtGoal = nxtGoal.substitute(asmnt)

                # this is already checked when the effected constraints
                # are computed. However, due to caching it could be that
                # new constraints were added since then.
                if self.inDB(nxtGoalId, nxtGoal):
                    continue

                if self.dbImplication(nxtGoalId, nxtGoal):
                    continue

            raise InvalidProof("Could not proof proof goal %s automatically." % (str(nxtGoalId)))


def autoProof(context, db, subgoals):
    goals = deque( ((nxtGoalId, nxtGoal) for nxtGoalId, nxtGoal in subgoals.items() if not nxtGoal.isProven) )
    subgoals.clear()

    if not goals:
        return

    prover = Autoprover(context, db, goals)
    prover()

    if prover.wasRUP:
        context.autoRUPstreak = getattr(context, "autoRUPstreak", 0) + 1
    else:
        context.autoRUPstreak = 0
