from recordclass import structclass
from enum import Enum
from veripb.rules import DummyRule, IsContradiction
from time import perf_counter
from veripb import InvalidProof

import logging

DBEntry = structclass("DBEntry","rule ruleNum constraint numUsed deleted")
Stats = structclass("Stats", "size space maxUsed")

class Context():
    pass

class DummyPropEngine():
    def attach(self, ineq):
        pass
    def detach(self, ineq):
        pass
    def attachTmp(self):
        raise RuntimeError()
    def isConflicting(self):
        raise RuntimeError()
    def reset(self):
        raise RuntimeError()


class Verifier():
    """
    Class to veryfi a complete proof.

    Attributese:
        db      the database of proof lines
        goals   index into db for lines that are needed for
                verification
        state   indicates the current state of the solve process
    """

    class State(Enum):
        CLEAN = 0,
        READ_PROOF = 1,
        MARKED_LINES = 2,
        DONE = 3

    class Iterator():
        def __init__(self, verifier):
            self.rules = enumerate(iter(verifier.rules))
            self.counter = 0
            self.rule = None
            self.lines = enumerate(iter(verifier.db))

        def __iter__(self):
            return self

        def __next__(self):
            if self.counter == 0:
                self.rule = next(self.rules)
                self.counter = self.rule[1].numConstraints()
            if self.counter != 0:
                self.counter -= 1
                line = next(self.lines)
            else:
                line = (None, None)
            return (
                self.rule,
                line,
                None if line is None else self.rule[1].numConstraints() - (self.counter + 1)
            )

    class Settings():
        def __init__(self, preset = None):
            self.setPreset(type(self).defaults(), unchecked = True)

            if preset is not None:
                self.setPreset(preset)

        def setPreset(self, preset, unchecked = False):
            for key, value in preset.items():
                if unchecked or hasattr(self, key):
                    setattr(self, key, value)
                else:
                    raise ValueError("Got unknown setting %s."%key)

        @staticmethod
        def defaults():
            return {
                "isInvariantsOn": False,
                "disableDeletion": True,
                "lazy": False,
                "trace": False,
            }

        def computeNumUse(self):
            return self.lazy or not self.disableDeletion

        @classmethod
        def addArgParser(cls, parser, name = "verifier"):
            defaults = cls.defaults()
            group = parser.add_argument_group(name)

            group.add_argument("--invariants", dest=name+".isInvariantsOn",
                action="store_true",
                default=defaults["isInvariantsOn"],
                help="Turn on invariant checking for debugging (might slow down cheking).")
            group.add_argument("--no-invariants", dest=name+".isInvariantsOn",
                action="store_false",
                help="Turn off invariant checking.")

            group.add_argument("--deletion", dest = name+".disableDeletion",
                action="store_false",
                default=defaults["disableDeletion"],
                help="turn on deletion of no longer needed constraitns to save space")
            group.add_argument("--no-deletion", dest = name+".disableDeletion",
                action="store_true",
                help="turn off deletion of no longer needed constraitns to save space")

            group.add_argument("--trace", dest = name+".trace",
                action="store_true",
                default=False,
                help="print a trace of derived constraints")

            group.add_argument("--lazy", dest = name+".lazy",
                action="store_true",
                default=defaults["lazy"],
                help="only compute constraints necessary for verification")
            group.add_argument("--no-lazy", dest = name+".lazy",
                action="store_false",
                help="compute all constraints")


        @classmethod
        def extract(cls, result, name = "verifier"):
            preset = dict()
            defaults = cls.defaults()
            for key in defaults:
                try:
                    preset[key] = getattr(result, name + "." + key)
                except AttributeError:
                    pass
            return cls(preset)

        def __repr__(self):
            return type(self).__name__ + repr(vars(self))



    def __init__(self, context, settings = None):
        if settings is not None:
            self.settings = settings
        else:
            self.settings = Verifier.Settings()

        if context is None:
            context = Context()
            context.propEngine = DummyPropEngine()
        self.context = context

    def __iter__(self):
        return Verifier.Iterator(self)

    def init(self, rules):
        # we want to start indexing with 1 so we add a dummy rule.
        dummy = DummyRule()
        assert dummy.numConstraints() == 1
        self.rules = [dummy] + rules
        self.db = list()
        self.goals = list()
        self.state = Verifier.State.CLEAN

        self._execCacheRule = None

    def antecedentIds(self, rule, ruleNum):
        def isActive(entry):
            if entry.deleted == False and entry.ruleNum < ruleNum:
                return True
            else:
                return entry.ruleNum < ruleNum and ruleNum < entry.deleted

        ids = rule.antecedentIDs()
        if ids == "all":
            return (i for i,e in enumerate(self.db) if isActive(e))
        return ids

    def mapRulesToDB(self):
        constraintNum = 0
        self.foundContradiction = False

        # Forward pass to find goals
        for ruleNum, rule in enumerate(self.rules):
            for i in range(rule.numConstraints()):
                self.db.append(DBEntry(
                    rule = rule,
                    ruleNum = ruleNum,
                    constraint = None,
                    numUsed = 0,
                    deleted = False))

                if rule.isGoal():
                    self.goals.append(([constraintNum], ruleNum))

                constraintNum += 1

            if rule.isGoal() and rule.numConstraints() == 0:
                if isinstance(rule, IsContradiction):
                    self.foundContradiction = True
                self.goals.append((self.antecedentIds(rule, ruleNum), ruleNum))

            self.checkAccess(rule.deleteConstraints(), ruleNum)
            for constraintId in rule.deleteConstraints():
                self.db[constraintId].deleted = ruleNum

        self.state = Verifier.State.READ_PROOF

    def checkAccess(self, goals, ruleNum):
        for goal in goals:
            if goal >= len(self.db):
                raise InvalidProof("Rule %i tries to access constraint "\
                    "(constraintId %i) which is not (yet?) created."%(ruleNum, goal))

    def markUsed(self):
        if not self.settings.computeNumUse():
            return

        todo = list()
        for goals, ruleNum in self.goals:
            self.checkAccess(goals, ruleNum)
            todo.extend(goals)

        # Backward pass to mark used rules
        while todo:
            goal = todo.pop()
            line = self.db[goal]

            line.numUsed += 1
            if line.numUsed == 1:
                antecedents = self.antecedentIds(line.rule, line.ruleNum)
                self.checkAccess(antecedents, line.ruleNum)
                antecedents = self.antecedentIds(line.rule, line.ruleNum)
                todo.extend(antecedents)

        self.state = Verifier.State.MARKED_LINES

    def decreaseUse(self, line):
        if not self.settings.computeNumUse():
            return
        line.numUsed -= 1
        assert line.numUsed >= 0
        if line.numUsed == 0:
            # free space of constraints that are no longer used
            if not self.settings.disableDeletion:
                self.context.propEngine.detach(line.constraint)
                line.constraint = None

    def execRule(self, rule, ruleNum, lineNum, numInRule = None):
        def get(otherLineNum):
            constraint = self.db[otherLineNum].constraint
            deleted = self.db[otherLineNum].deleted
            if constraint is None:
                if lineNum <= otherLineNum:
                    raise InvalidProof("Rule %i is trying to access constraint "\
                        "(constraintId %i), which is not derived, yet."%(ruleNum, otherLineNum))
                elif deleted is not False and deleted < ruleNum:
                    raise InvalidProof("Rule %i is trying to access constraint "\
                        "(constraintId %i), that was marked as save to delete by rule %i."\
                        %(ruleNum, otherLineNum, deleted))
                else:
                    # seeems like we did something wrong...
                    raise RuntimeError("Rule %i tried to access constraint (constraintId %i) "\
                        "that should be available, but is not."%(ruleNum, otherLineNum))

            return constraint

        assert rule is not None
        if self._execCacheRule is not rule:
            # logging.info("running rule: %s" %(rule))
            self._execCacheRule = rule
            antecedentIDs = self.antecedentIds(rule, ruleNum)
            antecedents = (get(i) for i in antecedentIDs)
            self._execCache = rule.compute(antecedents, self.context)

        if numInRule is not None:
            if self.settings.computeNumUse() \
                    and self.db[lineNum].numUsed > 0:
                # only decrease others use if we are used our selfs
                for i in self.antecedentIds(rule, ruleNum):
                    self.decreaseUse(self.db[i])

            return self._execCache[numInRule]

    def compute(self):
        # Forward pass to compute constraitns
        rule = None
        constraints = None
        antecedentIDs = None
        db = self.db

        for rule, line, numInRule in self:
            ruleNum, rule = rule
            lineNum, line = line

            try:
                lineInFile = "from line %i" % (rule.lineInFile)
            except AttributeError:
                lineInFile = "unknown line - %i-th step" %(ruleNum)

            if line is None:
                if rule.isGoal() or \
                        not self.settings.lazy:
                    self.execRule(rule, ruleNum, lineNum)
                    if self.settings.trace:
                        print("- (%s)"%(lineInFile))

            elif line.numUsed > 0 or \
                    not self.settings.lazy:
                assert numInRule is not None
                line.constraint = self.execRule(rule, ruleNum, lineNum, numInRule)
                self.context.propEngine.attach(line.constraint)
                if self.settings.trace and ruleNum > 0:
                    print("%(line)i (%(lineInFile)s): %(ineq)s"%{
                        "line": lineNum,
                        "lineInFile": lineInFile,
                        "ineq": self.context.ineqFactory.toString(line.constraint)
                    })
                if rule.isGoal():
                    self.decreaseUse(line)

            for i in rule.deleteConstraints():
                self.context.propEngine.detach(db[i].constraint)
                db[i].constraint = None

        self.state = Verifier.State.DONE

    def checkInvariants(self):
        if self.settings.isInvariantsOn:
            pass

    def __call__(self, rules):
        self.init(rules)
        self.checkInvariants()

        start_forward = perf_counter()
        self.mapRulesToDB()
        self.checkInvariants()
        logging.info("Forward Pass Time: %.2f" % (perf_counter() - start_forward))

        start_backward = perf_counter()
        self.markUsed()
        self.checkInvariants()
        logging.info("Backward Pass Time: %.2f" % (perf_counter() - start_backward))

        start_verify = perf_counter()
        self.compute()
        self.checkInvariants()
        logging.info("Verify Pass Time: %.2f" % (perf_counter() - start_verify))

        if not self.foundContradiction:
            logging.warn("The provided proof did not claim contradiction.")
