import itertools

from veripb.rules import DummyRule, IsContradiction
from veripb import InvalidProof
from veripb.timed_function import TimedFunction

import sys
import logging

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

def fileLineNum(ruleNum, rule):
    try:
        return "from line %i" % (rule.lineInFile)
    except AttributeError:
        return "unknown line - %i-th step" %(ruleNum)

class Verifier():
    """
    Class to veryfi a complete proof.

    Attributese:
        db      the database of proof lines
        goals   index into db for lines that are needed for
                verification
        state   indicates the current state of the solve process
    """

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

            group.add_argument("--trace", dest = name+".trace",
                action="store_true",
                default=False,
                help="print a trace of derived constraints")

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

    def antecedents(self, ids, ruleNum):
        if ids == "all":
            return (c for c in self.db if c is not None)
        else:
            for i in ids:
                if i >= len(self.db):
                    raise InvalidProof("Rule %i is trying to access constraint "\
                        "(constraintId %i), which is not derived, yet."\
                        %(ruleNum, i))

                constraint = self.db[i]
                if constraint is None:
                    raise InvalidProof("Rule %i is trying to access constraint "\
                        "(constraintId %i), that was marked as save to delete."\
                        %(ruleNum, i))

                yield constraint

    def __init__(self, context, settings = None):
        if settings is not None:
            self.settings = settings
        else:
            self.settings = Verifier.Settings()

        if context is None:
            context = Context()
            context.propEngine = DummyPropEngine()
        self.context = context

    @TimedFunction.time("propEngine::attach")
    def attach(self, constraint):
        self.context.propEngine.attach(constraint)

    @TimedFunction.time("propEngine::detach")
    def detach(self, constraint):
        self.context.propEngine.detach(constraint)

    def __call__(self, rules):
        self.db = list()
        self.foundContradiction = False

        for ruleNum, rule in enumerate(itertools.chain([DummyRule()], rules)):
            didPrint = False

            if isinstance(rule, IsContradiction):
                self.foundContradiction = True

            antecedents = self.antecedents(rule.antecedentIDs(), ruleNum)
            constraints = rule.compute(antecedents, self.context)

            for i, constraint in enumerate(constraints):
                lineNum = len(self.db) + i
                self.attach(constraint)
                if self.settings.trace and ruleNum > 0:
                    didPrint = True
                    print("%(line)i (%(lineInFile)s): %(ineq)s"%{
                        "line": lineNum,
                        "lineInFile": fileLineNum(ruleNum, rule),
                        "ineq": self.context.ineqFactory.toString(constraint)
                    })

            self.db.extend(constraints)

            deletedConstraints = list(rule.deleteConstraints())
            if self.settings.trace and len(deletedConstraints) > 0:
                didPrint = True
                print("- (%(lineInFile)s): deleting %(ineq)s"%{
                    "lineInFile": fileLineNum(ruleNum, rule),
                    "ineq": ", ".join(map(str,deletedConstraints))
                })

            for i in deletedConstraints:
                self.detach(self.db[i])

                # clean constraint to supress superficial warning
                constraint = None
                refcount = sys.getrefcount(self.db[i])
                if (refcount > 2):
                    logging.warn("Internal Warning: refcount of "
                        "deleted constraint too large (is %i), memory will "
                        "not be freed."%(refcount))
                self.db[i] = None

            if not didPrint == True and self.settings.trace and ruleNum > 0:
                print("- (%s)"%(fileLineNum(ruleNum, rule)))


        if not self.foundContradiction:
            logging.warn("The provided proof did not claim contradiction.")