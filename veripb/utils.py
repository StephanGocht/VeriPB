import argparse
import logging

import pyximport; pyximport.install(language_level=3)

from veripb import InvalidProof
from veripb import ParseError
from veripb.verifier import Verifier, Context
from veripb.parser import OPBParser
from veripb.rules import registered_rules
from veripb.rules import TimedFunction
from veripb.parser import RuleParser
from veripb.exceptions import ParseError
from veripb.optimized.constraints import PropEngine
from veripb.constraints import newDefaultFactory
from time import perf_counter

profile = False

if profile:
    import cProfile
    from pyprof2calltree import convert as convert2kcachegrind

def run(formulaFile, rulesFile, settings = None):
    if profile:
        pr = cProfile.Profile()
        pr.enable()


    rules = list(registered_rules)

    start_parse = perf_counter()

    context = Context()
    context.ineqFactory = newDefaultFactory()

    try:
        formula = OPBParser(context.ineqFactory).parse(formulaFile)
        numVars, numConstraints = formula[0]
        context.formula = formula[1]
    except ParseError as e:
        e.fileName = formulaFile.name
        raise e

    context.propEngine = PropEngine(numVars)

    try:
        rules = RuleParser(context).parse(rules, rulesFile)
    except ParseError as e:
        e.fileName = rulesFile.name
        raise e

    logging.info("Parsing Time: %.2f" % (perf_counter() - start_parse))
    verify = Verifier(
        context = context,
        settings = settings)
    verify(rules)

    TimedFunction.print_stats()

    if profile:
        pr.disable()
        convert2kcachegrind(pr.getstats(), 'pyprof.callgrind')

def runUI(*args):
    try:
        run(*args)
    except InvalidProof as e:
        print("Verification failed.")
        hint = str(e)
        if len(hint) > 0:
            print("Hint: %s" %(str(e)))
        return 1
    except ParseError as e:
        logging.error(e)
        exit(1)
    except NotImplementedError as e:
        logging.error(e)
        exit(1)
    except Exception as e:
        if logging.getLogger().getEffectiveLevel() != logging.DEBUG:
            logging.error("Sorry, there was an internal error. Please rerun with debugging "
                "and make a bug report.")
            exit(1)
        else:
            print("Sorry, there was an internal error. Because you are runing in "
                "debug mod I will give you the exception.")
            raise e
    else:
        print("Verification succeeded.")
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

    args = p.parse_args()

    verifyerSettings = Verifier.Settings.extract(args)

    logging.basicConfig(level=args.loglevel)

    return runUI(args.formula, args.derivation, verifyerSettings)
