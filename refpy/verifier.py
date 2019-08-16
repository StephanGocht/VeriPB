from recordclass import structclass
from enum import Enum
from refpy.rules import DummyRule, IsContradiction
from time import perf_counter

import logging
# import cProfile

DBEntry = structclass("DBEntry","rule ruleNum constraint numUsed deleted")
Stats = structclass("Stats", "size space maxUsed")

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
                "disableDeletion": False,
                "lazy": True,
                "trace": False,
            }

        @classmethod
        def addArgParsefr(cls, parser, name = "verifier"):
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



    def __init__(self, settings = None):
        if settings is not None:
            self.settings = settings
        else:
            self.settings = Verifier.Settings()

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
            return [i for i,e in enumerate(self.db) if isActive(e)]
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
                    self.goals.append(constraintNum)

                for constraintId in rule.deletedConstraints():
                    self.db[constraintId].deleted = ruleNum

                constraintNum += 1

            if rule.isGoal() and rule.numConstraints() == 0:
                if isinstance(rule, IsContradiction):
                    self.foundContradiction = True
                self.goals.extend(self.antecedentIds(rule, ruleNum))

        self.state = Verifier.State.READ_PROOF

    def markUsed(self):
        # Backward pass to mark used rules
        while self.goals:
            goal = self.goals.pop()
            line = self.db[goal]
            line.numUsed += 1
            if line.numUsed == 1:
                for antecedent in self.antecedentIds(line.rule, line.ruleNum):
                    # assert(antecedent < goal)
                    self.goals.append(antecedent)

        self.state = Verifier.State.MARKED_LINES

    def decreaseUse(self, line):
        line.numUsed -= 1
        assert line.numUsed >= 0
        if line.numUsed == 0:
            # free space of constraints that are no longer used
            if not self.settings.disableDeletion:
                line.constraint = None

    def execRule(self, rule, ruleNum, numInRule = None):
        assert rule is not None
        if self._execCacheRule is not rule:
            # logging.info("running rule: %s" %(rule))
            self._execCacheRule = rule
            antecedentIDs = self.antecedentIds(rule, ruleNum)

            self._execCache = rule.compute(
                [self.db[i].constraint for i in antecedentIDs])

            for i in antecedentIDs:
                self.decreaseUse(self.db[i])

        if numInRule is not None:
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
            if line is None:
                if rule.isGoal():
                    self.execRule(rule, ruleNum)
            elif line.numUsed > 0 or \
                    self.settings.lazy == False:
                assert numInRule is not None
                line.constraint = self.execRule(rule, ruleNum, numInRule)
                if self.settings.trace:
                    print("%i (rule %i): %s"%(lineNum, ruleNum, str(line.constraint)))
                if rule.isGoal():
                    self.decreaseUse(line)

        self.state = Verifier.State.DONE

    def checkInvariants(self):
        if self.settings.isInvariantsOn:
            pass

    def __call__(self, rules):
        # pr = cProfile.Profile()
        # pr.enable()

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

        # pr.disable()
        # pr.print_stats()