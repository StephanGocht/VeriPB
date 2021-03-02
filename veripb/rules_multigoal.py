from veripb import InvalidProof
from veripb.rules import Rule, EmptyRule, register_rule
from veripb.rules import ReversePolishNotation, IsContradiction
from veripb.rules_register import register_rule, dom_friendly_rules, rules_to_dict
from veripb.parser import OPBParser, WordParser, ParseContext

from veripb import verifier

from veripb.exceptions import ParseError

from veripb.timed_function import TimedFunction

from collections import deque

from veripb.autoproving import autoProof

class SubContextInfo():
    def __init__(self):
        self.toDelete = []
        self.toAdd = []
        self.previousRules = None
        self.callbacks = []
        self.subgoals = deque()

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
            ineqId, ineq = oldContext.subgoals[0]

            ineq = self.context.ineqFactory.toString(ineq)
            raise InvalidProof("Open subgoal not proven: %i:, %s"%(ineqId, ineq))

        return oldContext

    def getCurrent(self):
        return self.infos[-1]

class EndOfProof(EmptyRule):
    Ids = ["qed", "end"]

    @classmethod
    def parse(cls, line, context):
        subcontexts = SubContext.setup(context)
        try:
            subcontext = subcontexts.pop(checkSubgoals = False)
        except IndexError:
            raise ParseError("Nothing to end here.")
        return cls(subcontext)

    def __init__(self, subcontext):
        self.subcontext = subcontext

    def deleteConstraints(self):
        return self.subcontext.toDelete

    def compute(self, antecedents, context = None):
        autoProof(context, antecedents, self.subcontext.subgoals)
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

class SubProof(EmptyRule):
    Ids = ["proofgoal"]
    subRules = dom_friendly_rules() + [EndOfProof]

    # todo enforce only one def

    @classmethod
    def parse(cls, line, context):
        subcontexts = SubContext.setup(context)
        parentCtx = subcontexts.getCurrent()
        subContext = subcontexts.push()

        with WordParser(line) as words:
            myGoal = words.nextInt()

            if not parentCtx.subgoals:
                raise ValueError("No proofgoals left to proof.")

            if myGoal < 0:
                myGoal += context.firstFreeId

            found = False
            for nxtGoalId, nxtGoal in parentCtx.subgoals:
                if nxtGoalId == myGoal:
                    found = True
                    break

            if not found:
                raise ValueError("Invalid proofgoal.")

        return cls(subContext, parentCtx.subgoals, myGoal)



    def __init__(self, subContext, subgoals, myGoal):
        self.subContext = subContext
        self.subgoals = subgoals
        self.myGoal = myGoal

        f = lambda context, subContext: self.check(context)
        subContext.callbacks.append(f)



    def compute(self, antecedents, context):
        autoProof(context, antecedents, self.subgoals, self.myGoal)

        nxtGoalId, constraint = self.subgoals.popleft()
        assert(nxtGoalId == self.myGoal)
        return [constraint.copy().negated()]

    def antecedentIDs(self):
        return "all"

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
        self.nextId = context.firstFreeId
        self.autoProoved = False

    def addSubgoal(self, ineq, Id = None):
        if Id is None or Id == 0:
            # block of a new Id for subgoal
            Id = self.addAvailable(None)

        self.subContext.subgoals.append((Id, ineq))
        if self.displayGoals:
            ineqStr = self.ineqFactory.toString(ineq)
            print("  proofgoal %03i: %s"%(Id, ineqStr))

    def addAvailable(self, ineq):
        """add constraint available in sub proof"""
        Id = self.nextId
        self.constraints.append(ineq)

        self.nextId += 1
        return Id

    def autoProof(self, context, db):
        self.autoProoved = True

        for c in self.constraints:
            if c is not None:
                context.propEngine.attach(c)

        autoProof(context, db, self.subContext.subgoals)

        for c in self.constraints:
            if c is not None:
                context.propEngine.detach(c)

        self.constraints = [None] * len(self.constraints)
        self.constraints.extend(self.subContext.toAdd)
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