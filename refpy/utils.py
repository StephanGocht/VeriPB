import argparse
import logging
import parsy

from refpy import InvalidProof
from refpy import ParseError
from refpy.verifier import Verifier
from refpy.parser import getOPBParser
from refpy.rules import registered_rules
from refpy.rules import LoadFormulaWrapper
from refpy.parser import RuleParser
from refpy.exceptions import ParseError
from time import perf_counter

def run(formulaFile, rulesFile, settings = None):
    rules = list(registered_rules)

    start_parse = perf_counter()
    try:
        formula = getOPBParser().parse(formulaFile.read())
        numVars, numConstraints = formula[0]
        constraints = formula[1]
    except parsy.ParseError as e:
        raise ParseError(e, formulaFile.name)

    rules.append(LoadFormulaWrapper(constraints))

    try:
        rules = RuleParser().parse(rules, rulesFile)
    except ParseError as e:
        e.fileName = rulesFile.name
        raise e

    logging.info("Parsing Time: %.2f" % (perf_counter() - start_parse))
    verify = Verifier(settings)
    verify(rules)

def runUI(*args):
    try:
        run(*args)
    except InvalidProof as e:
        print("Verification failed.")
    except ParseError as e:
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

    runUI(args.formula, args.derivation, verifyerSettings)