import argparse
import logging

import os
# import pyximport; pyximport.install(
#     language_level=3,
#     build_dir=os.path.join(os.path.dirname(os.path.abspath(__file__)), "__pycache__/pyximport")
# )

from veripb import InvalidProof
from veripb import ParseError
from veripb.verifier import Verifier, Context
from veripb.parser import OPBParser, CNFParser
from veripb.drat import DRATParser
from veripb.rules_register import get_registered_rules
from veripb.timed_function import TimedFunction
from veripb.parser import RuleParser
from veripb.exceptions import ParseError
from veripb.optimized.constraints import PropEngine as CppPropEngine,PropEngineBigInt
from veripb.optimized.parsing import parseOpb,parseOpbBigInt
from veripb.constraints import PropEngine,CppIneqFactory,BigIntIneqFactory
from time import perf_counter

from veripb.rules_dominance import stats as dominance_stats

profile = True

if profile:
    import cProfile
    from pyprof2calltree import convert as convert2kcachegrind
    from guppy import hpy

@TimedFunction.time("LoadFormula")
def loadFormula(file, parser, varMgr):
    formula = parser(file, varMgr)
    return {
        "numVariables": formula.maxVar,
        "constraints": formula.getConstraints(),
        "objective": objectiveToDict(formula)
    }

def objectiveToDict(formula):
    if (formula.hasObjective):
        coeffs = formula.objectiveCoeffs
        variables = formula.objectiveVars
        return {var: coeff for (coeff,var) in zip(coeffs, variables)}
    else:
        return None

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
            "drat": False,
            "cnf": False,
            "arbitraryPrecision": False,
            "enableFreeNames": True,
            "printStats": False
        }

    def computeNumUse(self):
        return self.lazy or not self.disableDeletion

    @classmethod
    def addArgParser(cls, parser, name = "misc"):
        defaults = cls.defaults()
        group = parser.add_argument_group(name)

        group.add_argument(
            '--drat',
            help="Process CNF with DRAT proof.",
            action="store_true", dest=name+".drat", default=False
        )

        group.add_argument(
            '--cnf',
            help="Process CNF with PB proof.",
            action="store_true", dest=name+".cnf", default=False
        )

        group.add_argument("--arbitraryPrecision",
            action="store_true",
            default=defaults["arbitraryPrecision"],
            help="Turn on arbitrary precision. Slows down proof checking.",
            dest=name+".arbitraryPrecision")
        group.add_argument("--no-arbitraryPrecision",
            action="store_false",
            help="Turn off arbitrary precision.",
            dest=name+".arbitraryPrecision")

        group.add_argument("--freeNames",
            action="store_true",
            default=defaults["enableFreeNames"],
            help="Enable use of arbitrary variable names.",
            dest=name+".enableFreeNames")
        group.add_argument("--no-freeNames",
            action="store_false",
            help="Disable use of arbitrary variable names.",
            dest=name+".enableFreeNames")

        group.add_argument("--stats",
            action="store_true",
            default=defaults["printStats"],
            help="Print statistics on terminations.",
            dest=name+".printStats")
        group.add_argument("--no-stats",
            action="store_false",
            help="Disable printing of statistics on terminations.",
            dest=name+".printStats")

    @classmethod
    def extract(cls, result, name = "misc"):
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

def run(formulaFile, rulesFile, verifierSettings = None, miscSettings = Settings()):
    if profile:
        pr = cProfile.Profile()
        pr.enable()

    if verifierSettings == None:
        verifierSettings = Verifier.Settings()

    TimedFunction.startTotalTimer()

    rules = list(get_registered_rules())

    context = Context()
    if miscSettings.arbitraryPrecision:
        context.ineqFactory = BigIntIneqFactory(miscSettings.enableFreeNames)
    else:
        context.ineqFactory = CppIneqFactory(miscSettings.enableFreeNames)

    try:
        if miscSettings.drat or miscSettings.cnf:
            formula = CNFParser(context.ineqFactory).parse(formulaFile)
        else:
            if miscSettings.arbitraryPrecision:
                parser = parseOpbBigInt
            else:
                parser = parseOpb
            formula = loadFormula(formulaFile.name, parser, context.ineqFactory.varNameMgr)
    except ParseError as e:
        e.fileName = formulaFile.name
        raise e

    context.formula = formula["constraints"]
    context.objective = formula["objective"]

    def newPropEngine():
        if miscSettings.arbitraryPrecision:
            return PropEngineBigInt(formula["numVariables"])
        else:
            return CppPropEngine(formula["numVariables"])

    context.propEngine = newPropEngine()
    context.newPropEngine = newPropEngine


    verify = Verifier(
        context = context,
        settings = verifierSettings)

    try:
        if verifierSettings.progressBar:
            context.ruleCount = ruleParser.numRules(rulesFile)

        if not miscSettings.drat:
            ruleParser = RuleParser(context)
            rules = ruleParser.parse(rules, rulesFile, dumpLine = verifierSettings.trace)
        else:
            ruleParser = DRATParser(context)
            rules = ruleParser.parse(rulesFile)

        return verify(rules)
    except ParseError as e:
        e.fileName = rulesFile.name
        raise e
    finally:
        if miscSettings.printStats:
            TimedFunction.print_stats()
            context.propEngine.printStats()

            dominance_stats.print_stats()

        if profile:
            pr.disable()
            convert2kcachegrind(pr.getstats(), 'callgrind.out.py.profile')

def runUI(*args, **kwargs):
    try:
        result = run(*args, **kwargs)
        result.print()

    except InvalidProof as e:
        print("Verification failed.")
        line = getattr(e, "lineInFile", None)
        where = ""
        if line is not None:
            print("Failed in proof file line %i."%(line))
        hint = str(e)


        if len(hint) > 0:
            print("Hint: %s" %(str(e)))
        return 1

    except ParseError as e:
        logging.error(e)
        return 1

    except NotImplementedError as e:
        logging.error("Not Implemented: %s" % str(e))
        return 1

    except Exception as e:
        if logging.getLogger().getEffectiveLevel() != logging.DEBUG:
            logging.error("Sorry, there was an internal error. Please rerun with debugging "
                "and make a bug report.")
            return 1
        else:
            print("Sorry, there was an internal error. Because you are running in "
                "debug mode I will give you the exception.")
            raise e

    return 0


def run_cmd_main():
    p = argparse.ArgumentParser(
        description = """Command line tool to verify derivation
            graphs. See Readme.md for a description of the file
            format.""")
    p.add_argument("formula", help="Formula containing axioms.", type=argparse.FileType('r'))
    p.add_argument("derivation", help="Refutation / Proof Log.", type=argparse.FileType('r'))
    p.add_argument(
        '-d', '--debug',
        help="Print lots of debugging statements.",
        action="store_const", dest="loglevel", const=logging.DEBUG,
        default=logging.INFO,
    )
    p.add_argument(
        '-v', '--verbose',
        help="Be verbose",
        action="store_const", dest="loglevel", const=logging.INFO,
    )

    Verifier.Settings.addArgParser(p)
    Settings.addArgParser(p)

    args = p.parse_args()

    verifyerSettings = Verifier.Settings.extract(args)
    miscSettings = Settings.extract(args)

    logging.basicConfig(level=args.loglevel)

    return runUI(args.formula, args.derivation, verifyerSettings, miscSettings)
