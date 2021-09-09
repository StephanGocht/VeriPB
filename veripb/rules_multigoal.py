from veripb import InvalidProof
from veripb.rules import Rule, EmptyRule, register_rule
from veripb.rules import ReversePolishNotation, IsContradiction
from veripb.rules_register import register_rule, dom_friendly_rules, rules_to_dict
from veripb.parser import OPBParser, MaybeWordParser, ParseContext

from veripb import verifier

from veripb.exceptions import ParseError

from veripb.timed_function import TimedFunction

from collections import deque

from veripb.autoproving import autoProof

from veripb.optimized.constraints import maxId as getMaxConstraintId
constraintMaxId = getMaxConstraintId()

class SubContextInfo():
    def __init__(self):
        self.toDelete = []
        self.toAdd = []
        self.previousRules = None
        self.callbacks = []
        self.subgoals = dict()

    def addToDelete(self, ineqs):
        self.toDelete.extend(ineqs)

class SubContext():
    @classmethod
    def setup(cls, context):
        try:
            return context.subContexts
        except AttributeError:
            context.subContexts = cls(context)
            return context.subContexts

    def __init__(self, context):
        self.infos = []
        self.context = context

        f = lambda ineqs, context: self.addToDelete(ineqs)
        context.addIneqListener.append(f)

    def __bool__(self):
        return bool(self.infos)

    def addToDelete(self, ineqs):
        if len(self.infos) > 0:
            self.infos[-1].addToDelete(ineqs)

    def push(self):
        newSubContext = SubContextInfo()
        self.infos.append(newSubContext)
        return self.infos[-1]

    def pop(self, checkSubgoals = True):
        oldContext = self.infos.pop()
        for callback in oldContext.callbacks:
            callback(self.context, oldContext)

        if checkSubgoals and len(oldContext.subgoals) > 0:
            for Id, subgoal in oldContext.subgoals.items():
                raise InvalidProof("Open subgoal not proven: %s: %s"%(str(Id), subgoal.toString(self.context.ineqFactory)))

        return oldContext

    def getCurrent(self):
        return self.infos[-1]

class EndOfProof(EmptyRule):
    Ids = ["qed", "end"]

    @classmethod
    def parse(cls, line, context):
        subContexts = SubContext.setup(context)
        if not subContexts:
            raise ParseError("No subcontext to end here.")

        return cls(subContexts.getCurrent())

    def __init__(self, subcontext):
        self.subcontext = subcontext

    def deleteConstraints(self):
        return self.subcontext.toDelete

    def compute(self, antecedents, context):
        autoProof(context, antecedents, self.subcontext.subgoals)
        subContexts = SubContext.setup(context)
        poped = subContexts.pop()
        assert(self.subcontext == poped)
        return self.subcontext.toAdd

    def antecedentIDs(self):
        return "all"

    def allowedRules(self, context, currentRules):
        if self.subcontext.previousRules is not None:
            return self.subcontext.previousRules
        else:
            return currentRules

    def numConstraints(self):
        return len(self.subcontext.toAdd)

class NegatedSubGoals:
    def __init__(self, constraints):
        self.constraints = constraints
        self.isProven = False

    def getAsLeftHand(self):
        return self.constraints

    def getAsRightHand(self):
        return None

    def toString(self, ineqFactory):
        constraintsString = " ".join([ineqFactory.toString(constraint) for constraint in self.constraints])
        return "not [%s]" % constraintsString

    def isProven():
        return False

class SubGoal:
    def __init__(self, constraint):
        self.constraint = constraint
        self.isProven = False

    def getAsLeftHand(self):
        return [self.constraint.copy().negated()]

    def getAsRightHand(self):
        return self.constraint

    def toString(self, ineqFactory):
        return ineqFactory.toString(self.constraint)


class SubProof(EmptyRule):
    Ids = ["proofgoal"]

    # todo enforce only one def

    @classmethod
    def parse(cls, line, context):
        subcontexts = SubContext.setup(context)
        subContext = subcontexts.getCurrent()

        with MaybeWordParser(line) as words:
            myGoal = next(words)
            if myGoal[0] != "#":
                myGoal = int(myGoal)

            if not subContext.subgoals:
                raise ValueError("No proofgoals left to proof.")

            if myGoal not in subContext.subgoals:
                raise ValueError("Invalid proofgoal.")

        return cls(subContext.subgoals, myGoal)

    def __init__(self, subgoals, myGoal):
        self.subRules = dom_friendly_rules() + [EndOfProof]
        self.myGoal = myGoal
        self.subgoals = subgoals

    def compute(self, antecedents, context):
        subContexts = SubContext.setup(context)
        self.subContext = subContexts.push()

        f = lambda context, subContext: self.check(context)
        self.subContext.callbacks.append(f)

        subgoal = self.subgoals[self.myGoal]
        del self.subgoals[self.myGoal]

        return subgoal.getAsLeftHand()

    def antecedentIDs(self):
        return []

    def numConstraints(self):
        return 1

    def allowedRules(self, context, currentRules):
        self.subContext.previousRules = currentRules
        return rules_to_dict(self.subRules)

    def check(self, context):
        if not getattr(context, "containsContradiction", False):
            raise InvalidProof("Sub proof did not show contradiction.")

        context.containsContradiction = False

class MultiGoalRule(EmptyRule):
    subRules = [EndOfProof, SubProof]
    subProofBegin = "begin"

    @classmethod
    def parseHasExplicitSubproof(cls, words):
        try:
            nxt = next(words)
            if nxt != cls.subProofBegin:
                raise ValueError("Unexpected word, expected 'begin'")
            return True
        except StopIteration:
            return False

    def __init__(self, context):
        self.subContexts = SubContext.setup(context)
        self.subContext = self.subContexts.push()
        self.displayGoals = context.verifierSettings.trace
        self.constraints = []
        self.ineqFactory = context.ineqFactory
        self.nextId = 1
        self.autoProoved = False

    def addSubgoal(self, goal, Id = None):
        if Id is None or Id == constraintMaxId:
            # the goal does not relate to an existing constraint
            Id = "#%i" % (self.nextId)
            self.nextId += 1

        assert(Id not in self.subContext.subgoals)
        self.subContext.subgoals[Id] = goal
        if self.displayGoals:
            ineqStr = goal.toString(self.ineqFactory)
            if isinstance(Id, int):
                Id = "%03i" % (Id)
            print("  proofgoal %s: %s"%(Id, ineqStr))

    def addAvailable(self, ineq):
        """add constraint available in sub proof"""
        self.constraints.append(ineq)

    def autoProof(self, context, db):
        self.autoProoved = True

        if self.subContext.subgoals:
            added = list()
            for c in self.constraints:
                if c is not None:
                    added.append(context.propEngine.attach(c, 0))

            try:
                autoProof(context, db, self.subContext.subgoals)
            finally:
                for c in added:
                    if c is not None:
                        context.propEngine.detach(c, 0)

        self.constraints = self.subContext.toAdd
        self.subContexts.pop()

    def addIntroduced(self, ineq):
        """add constraint introduced after all subgoals are proven"""
        self.subContext.toAdd.append(ineq)

    def compute(self, antecedents, context = None):
        return self.constraints

    def numConstraints(self):
        return len(self.constraints)

    def allowedRules(self, context, currentRules):
        if not self.autoProoved:
            self.subContext.previousRules = currentRules
            return rules_to_dict(self.subRules)
        else:
            return currentRules