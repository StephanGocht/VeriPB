import argparse
import logging

import os
import pyximport; pyximport.install(
    language_level=3,
    build_dir=os.path.join(os.path.dirname(os.path.abspath(__file__)), "__pycache__/pyximport")
)

from veripb import InvalidProof
from veripb import ParseError
from veripb.verifier import Verifier, Context
from veripb.parser import OPBParser
from veripb.rules import registered_rules
from veripb.rules import TimedFunction
from veripb.parser import RuleParser
from veripb.exceptions import ParseError
from veripb.optimized.constraints import PropEngine as CppPropEngine
from veripb.constraints import PropEngine,CppIneqFactory,IneqFactory
from time import perf_counter

profile = False

if profile:
    import cProfile
    from pyprof2calltree import convert as convert2kcachegrind
    from guppy import hpy

def run(formulaFile, rulesFile, settings = None, arbitraryPrecision = False):
    if profile:
        pr = cProfile.Profile()
        pr.enable()

    if settings == None:
        settings = Verifier.Settings()

    TimedFunction.startTotalTimer()

    rules = list(registered_rules)

    context = Context()
    if arbitraryPrecision:
        context.ineqFactory = IneqFactory()
    else:
        context.ineqFactory = CppIneqFactory()

    try:
        formula = OPBParser(context.ineqFactory).parse(formulaFile)
        context.formula = formula["constraints"]
        context.objective = formula["objective"]
    except ParseError as e:
        e.fileName = formulaFile.name
        raise e

    if arbitraryPrecision:
        context.propEngine = PropEngine()
    else:
        context.propEngine = CppPropEngine(formula["numVariables"])


    verify = Verifier(
        context = context,
        settings = settings)

    try:
        ruleParser = RuleParser(context)
        if settings.progressBar:
            context.ruleCount = ruleParser.numRules(rulesFile)
        rules = ruleParser.parse(rules, rulesFile, dumpLine = settings.trace)
        return verify(rules)
    except ParseError as e:
        e.fileName = rulesFile.name
        raise e
    finally:
        TimedFunction.print_stats()
        context.propEngine.printStats()

        if profile:
            pr.disable()
            convert2kcachegrind(pr.getstats(), 'pyprof.callgrind')

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

    p.add_argument("--arbitraryPrecision",
        action="store_true",
        default=False,
        help="Turn on arbitrary precision. Experimental, does not work with unit propagation or solution checking.")
    p.add_argument("--no-arbitraryPrecision",
        action="store_false",
        help="Turn off arbitrary precision.")

    Verifier.Settings.addArgParser(p)

    args = p.parse_args()

    verifyerSettings = Verifier.Settings.extract(args)

    logging.basicConfig(level=args.loglevel)

    return runUI(args.formula, args.derivation, verifyerSettings, arbitraryPrecision = args.arbitraryPrecision)
