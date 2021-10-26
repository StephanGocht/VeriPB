import itertools

from veripb.rules import DummyRule, IsContradiction
from veripb import InvalidProof
from veripb.timed_function import TimedFunction

from string import Template

import sys
import logging
import time
import argparse

import gc

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

# Print iterations progress
def printProgressBar (iteration, total, start_time, prefix = '', suffix = '', decimals = 1, length = 100, fill = '█', printEnd = "\r", stream = sys.stderr):
    """
    Call in a loop to create terminal progress bar
    @params:
        iteration   - Required  : current iteration (Int)
        total       - Required  : total iterations (Int)
        prefix      - Optional  : prefix string (Str)
        suffix      - Optional  : suffix string (Str)
        decimals    - Optional  : positive number of decimals in percent complete (Int)
        length      - Optional  : character length of bar (Int)
        fill        - Optional  : bar fill character (Str)
        printEnd    - Optional  : end character (e.g. "\r", "\r\n") (Str)
        stream      - Optional  : output stream (File object)
    """
    percent = ("{0:." + str(decimals) + "f}").format(100 * (iteration / float(total)))
    time_left = 0 if iteration==0 else int(round((time.time() - start_time)*(float(total)/iteration-1)))
    filledLength = int(length * iteration // total)
    bar = fill * filledLength + '-' * (length - filledLength)
    print('\r%s |%s| %s%% %ds remaining %s' % (prefix, bar, percent, time_left, suffix), end = printEnd,file=stream)
    # Print New Line on Complete
    if iteration == total:
        print(file=stream)

class VerificationResult():
    def __init__(self):
        self.isSuccessfull = False
        self.usesAssumptions = False
        self.containsContradiction = False
        self.requireUnsat = None

    def print(self):
        if not self.containsContradiction and self.requireUnsat is None:
            logging.warn("The provided proof did not claim contradiction.")

        if self.usesAssumptions:
            logging.warn("Proof is based on unjustified assumptions.")

        print("Verification succeeded.")

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
                "progressBar": False,
                "proofGraph": None,
                "requireUnsat": None,
                "isCheckDeletionOn": False,
                "useColor": False
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

            group.add_argument("--checkDeletion", dest=name+".isCheckDeletionOn",
                action="store_true",
                default=defaults["isCheckDeletionOn"],
                help="Turn on checking deletions in core set via RUP.")
            group.add_argument("--no-checkDeletion", dest=name+".isCheckDeletionOn",
                action="store_false",
                help="Disable checking deletions in core, but forbids deletion when order is loaded.")

            group.add_argument("--requireUnsat", dest=name+".requireUnsat",
                action="store_true",
                default=defaults["requireUnsat"],
                help="Require proof to contain contradiction.")
            group.add_argument("--no-requireUnsat", dest=name+".requireUnsat",
                action="store_false",
                help="Do not require proof to contain contradiction, supress warning.")

            group.add_argument("--useColor", dest=name+".useColor",
                action="store_true",
                default=defaults["useColor"],
                help="Require proof to contain contradiction.")
            group.add_argument("--no-useColor", dest=name+".useColor",
                action="store_false",
                help="Do not require proof to contain contradiction, supress warning.")

            group.add_argument("--trace", dest = name+".trace",
                action="store_true",
                default=False,
                help="Print a trace of derived constraints.")

            group.add_argument("--proofGraph", dest = name+".proofGraph",
                type=argparse.FileType('w'),
                default=defaults["proofGraph"],
                help="Write proof graph to given file.")

            group.add_argument("--progressBar", dest = name+".progressBar",
                action="store_true",
                default=False,
                help="Print a progress bar to stderr.")

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

    def print_stats(self):
        print("c statistic: num rules checked: %i"%(self.checked_rules))

    def print(self, *args, **kwargs):
        if not self.settings.useColor:
            print(*args, **kwargs)
        else:
            colors = {
                "cid":"\u001b[36m",
                "reset":"\u001b[0m",
                "ienq":"\u001b[34;1m"
            }
            args = [Template(arg).substitute(**colors) for arg in args]
            print(*args, **kwargs)

    def antecedents(self, ids, ruleNum):
        if ids == "all":
            for Id, c in enumerate(self.db):
                if c is not None:
                    # this is inconsitent now as we get (Id, c) for
                    # "all" but the constraint directly if a list of
                    # ids is provided. I don' have time to fix this
                    # now and probably nobody (also not future me)
                    # will notice as you ever only want the Ids if you
                    # don't know them already.
                    yield (Id, c)
        else:
            for i in ids:
                if i >= len(self.db):
                    raise InvalidProof("Rule %i is trying to access constraint "\
                        "(constraintId %i), which is not derived, yet."\
                        %(ruleNum, i))

                constraint = self.db[i]
                if constraint is None:
                    raise InvalidProof("Rule %i is trying to access constraint "\
                        "(constraintId %i), that was marked as safe to delete."\
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

        context.verifierSettings = self.settings
        self.context = context
        self.checked_rules = 0

    @TimedFunction.time("propEngine.attach")
    def attach(self, constraint, constraintId):
        return self.context.propEngine.attach(constraint, constraintId)

    @TimedFunction.time("propEngine.detach")
    def detach(self, constraint, constraintId):
        return self.context.propEngine.detach(constraint, constraintId)

    def handleRule(self, ruleNum, rule):
        self.checked_rules += 1
        if self.settings.progressBar:
            printProgressBar(ruleNum,self.context.ruleCount,self.start_time,length=50)

        didPrint = False

        antecedents = self.antecedents(rule.antecedentIDs(), ruleNum)
        constraints = rule.compute(antecedents, self.context)

        for constraint in constraints:
            if constraint is None:
                continue

            constraintId = len(self.db)
            constraint = self.attach(constraint, constraintId)
            if self.settings.trace and ruleNum > 0:
                didPrint = True
                self.print("  ConstraintId ${cid}%(line)03d${reset}: ${ienq}%(ineq)s${reset}"%{
                    "line": constraintId,
                    "ineq": self.context.ineqFactory.toString(constraint)
                })
            if self.settings.proofGraph is not None and ruleNum > 0:
                ids = rule.antecedentIDs()
                if ids == "all":
                    ids = (i for (i,c) in enumerate(self.db) if c is not None and i > 0)
                f = self.settings.proofGraph
                print("%(ineq)s ; %(line)d = %(antecedents)s"%{
                        "line": constraintId,
                        "ineq": self.context.ineqFactory.toString(constraint),
                        "antecedents": " ".join(map(str,ids))
                    }, file=f)

            self.db.append(constraint)

        deletedConstraints = [i for i in rule.deleteConstraints() if self.db[i] is not None]
        if self.settings.trace and len(deletedConstraints) > 0:
            didPrint = True
            print("  ConstraintId  - : deleting %(ineq)s"%{
                "ineq": ", ".join(map(str,deletedConstraints))
            })

        deleted = dict()
        for i in deletedConstraints:
            ineq = self.db[i]
            if ineq is None:
                continue

            self.db[i] = None
            wasLastReference = self.detach(ineq, i)

            if wasLastReference and ineq.isCoreConstraint:
                if self.settings.isCheckDeletionOn:
                    if not ineq.rupCheck(self.context.propEngine, True):
                        raise InvalidProof("Could not verify deletion of core constraint %s",
                            self.context.ineqFactory.toString(ineq))
                else:
                    orderContext = getattr(self.context, "orderContext", None)
                    if orderContext is not None and len(orderContext.activeOrder.vars) > 0:
                        if not ineq.rupCheck(self.context.propEngine, True):
                            raise InvalidProof("Could not verify deletion of core constraint while order was loaded.")

            deleted[id(ineq)] = ineq


        # clean up references, to not get spicious warnings
        constraint = None
        antecedents = None
        for ineq in deleted.values():
            refcount = sys.getrefcount(ineq)
            attachCount = self.context.propEngine.attachCount(ineq)
            if (attachCount == 0 and refcount > 4):
                # todo: refcount should be at-most 3, except for
                # constraints that apear in the formula or in dominance proofs.
                #logging.warning
                print("Internal Warning: refcount of "
                    "deleted constraint too large (is %i), memory will "
                    "not be freed."%(refcount))
                print(self.context.ineqFactory.toString(ineq))
                # import gc
                # for refer in gc.get_referrers(self.db[i]):
                #     print(refer)

        # if not didPrint == True and self.settings.trace and ruleNum > 0:
        #    print("  ConstraintId  - : check passed")

    def __call__(self, rules):
        rules = iter(rules)
        self.context.rules = rules

        self.db = list()
        self.result = VerificationResult()
        self.result.requireUnsat = self.settings.requireUnsat;

        if self.settings.trace:
            print()
            print("=== begin trace ===")

        if self.settings.progressBar:
            self.start_time = time.time()

        for ruleNum, rule in enumerate(itertools.chain([DummyRule()], rules)):
            try:
                self.handleRule(ruleNum, rule)
            except InvalidProof as e:
                e.lineInFile = rule.lineInFile
                raise e

        self.result.usesAssumptions = getattr(self.context, "usesAssumptions", False)
        self.result.containsContradiction = getattr(self.context, "containsContradiction", False)

        if self.settings.requireUnsat and not self.result.containsContradiction:
            raise InvalidProof("Proof does not contain contradiction!")

        if self.settings.trace:
            print("=== end trace ===")
            print()

        return self.result
