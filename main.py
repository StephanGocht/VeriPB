import argparse
from collections import namedtuple
from recordclass import structclass
#todo recordclass requires install
from math import copysign, ceil
from enum import Enum
import parsy

import logging

from functools import partial


Term = structclass("Term","coefficient variable")

class AllBooleanUpperBound():
    """
    Stub that needs to be replaced if we want to start handeling
    integer constraitns.
    """
    def __getitem__(self, key):
        return 1

class Inequality():
    """
    Constraint representing sum of terms greater or equal degree.
    Terms are stored in normalized form, i.e. negated literals but no
    negated coefficient. Variables are represented as ingtegers
    greater 0, the sign of the literal is stored in the sign of the
    integer representing the variable, i.e. x ~ 2 then not x ~ -2.

    For integers 0 <= x <= d the not x is defined as d - x. Note that
    a change in the upperbound invalidates the stored constraint.
    """

    @staticmethod
    def fromParsy(t):
        result = list()

        if isinstance(t, list):
            result.append(Inequality([Term(1,l) for l in t], 1))
        else:
            terms, eq, degree = t

            if eq == "=":
                result.append(Inequality([Term(-a,x) for a,x in terms], -degree))

            result.append(Inequality([Term(a,x) for a,x in terms], degree))

        return parsy.success(result)

    @staticmethod
    def getOPBParser(allowEq = True):
        return getOPBConstraintParser(allowEq = True).bind(Inequality.fromParsy)

    @staticmethod
    def getCNFParser():
        return getCNFConstraintParser().bind(Inequality.fromParsy)

    def __init__(self, terms = list(), degree = 0, variableUpperBounds = AllBooleanUpperBound()):
        self.degree = degree
        self.terms = terms
        self.variableUpperBounds = variableUpperBounds
        self.normalize()

    def normalize(self):
        for term in self.terms:
            if term.coefficient < 0:
                term.variable = -term.variable
                term.coefficient = abs(term.coefficient)
                self.degree += self.variableUpperBounds[abs(term.variable)] * term.coefficient

        self.terms.sort(key = lambda x: abs(x.variable))

    def addWithFactor(self, factor, other):
        self.degree += factor * other.degree
        result = list()

        otherTerms = map(lambda x: Term(factor * x.coefficient, x.variable), other.terms)
        myTerms = iter(self.terms)

        other = next(otherTerms, None)
        my    = next(myTerms, None)
        while (other is not None or my is not None):
            print(other, my)
            if other is None:
                if my is not None:
                    result.append(my)
                    result.extend(myTerms)
                break

            if my is None:
                if other is not None:
                    result.append(other)
                    result.extend(otherTerms)
                break

            if (abs(my.variable) == abs(other.variable)):
                a = copysign(my.coefficient, my.variable)
                b = copysign(other.coefficient, other.variable)
                newCoefficient = a + b
                newVariable = copysign(abs(my.variable), newCoefficient)
                newCoefficient = abs(newCoefficient)
                cancellation = max(0, max(my.coefficient, other.coefficient) - newCoefficient)
                self.degree -= cancellation * self.variableUpperBounds[abs(my.variable)]
                if newCoefficient != 0:
                    result.append(Term(newCoefficient, newVariable))
                other = next(otherTerms, None)
                my    = next(myTerms, None)

            elif abs(my.variable) < abs(other.variable):
                result.append(my)
                my    = next(myTerms, None)

            else:
                result.append(other)
                other = next(otherTerms, None)

        self.terms = result

    def saturate(self):
        for term in self.terms:
            term.coefficient = min(
                term.coefficient,
                self.degree)

    def divide(self, d):
        for term in self.terms:
            term.coefficient = ceil(term.coefficient / d)
        self.degree = ceil(self.degree / d)

    def isContradiction(self):
        slack = -self.degree
        for term in self.terms:
            slack += term.coefficient
        return slack < 0


    def __eq__(self, other):
        # note that terms is assumed to be soreted
        return self.degree == other.degree \
            and self.terms == other.terms

    def __str__(self):
        return " ".join(
            ["%+ix%i"%(term.coefficient, term.variable)
                for term in self.terms]) + \
            " >= %i" % self.degree

    def __repr__(self):
        return str(self)

registered_rules = []
def register_rule(rule):
    registered_rules.append(rule)
    return rule

class Rule():
    Id = None

    @staticmethod
    def getParser():
        """
        Returns:
            A parser that creates this rule from the string following the Id
        """
        raise NotImplementedError()

    def compute(self, antecedents):
        """
        Performs the action of the rule, e.g. compute new constraints or perform
        checks on the current proof.

        Args:
            antecedents (list): Antecedents in the order given by antecedentIDs

        Returns:
            computed constraints or empty list
        """
        raise NotImplementedError()

    def numConstraints(self):
        """
        Returns:
            number of constraints that will be produced by this rule
        """
        raise NotImplementedError()

    def antecedentIDs(self):
        """
        Return a list of indices of used antecednts
        """
        raise NotImplementedError()

    def isGoal(self):
        return False

    def __str__(self):
        return type(self).__name__

class DummyRule(Rule):
    def compute(self, db):
        return [Inequality([], 0)]

    def numConstraints(self):
        return 1

    def antecedentIDs(self):
        return []

class RuleParser():
    def checkIdentifier(self, Id):
        if not Id in self.identifier:
            return parsy.fail("Unsuported rule.")

        return parsy.success(Id)


    def parse(self, rules, stream):
        logging.info("Available Rules: %s"%(", ".join(["%s"%(rule.Id) for rule in rules])))

        self.identifier = set([rule.Id for rule in rules])

        header = parsy.regex(r"refutation graph using").\
            then(
                parsy.regex(r" +").desc("space").\
                    then(
                        parsy.regex(r"[^0 ]+").desc("rule identifier").\
                        bind(partial(RuleParser.checkIdentifier, self))
                    ).\
                    many()
            ).skip(
                parsy.regex(r" *0 *\n?").desc("0 at end of line")
            )

        skipBlankLines = parsy.regex(r"\s*")
        singleRules = [skipBlankLines >> parsy.regex(rule.Id).desc("rule identifier") >> rule.getParser() << skipBlankLines for rule in rules]
        anyRuleParser = parsy.alt(*singleRules)

        parser = header >> anyRuleParser.many()

        return parser.parse(stream)

def getCNFConstraintParser():
    space    = parsy.regex(r" +").desc("space")
    literal  = parsy.regex(r"[+-]?[1-9][0-9]*").desc("literal").map(int)
    eol      = parsy.regex(r"0").desc("end of line zero")

    return (literal << space).many() << eol

def getOPBConstraintParser(allowEq = True):
    space    = parsy.regex(r" +").desc("space").optional()
    coeff    = parsy.regex(r"[+-]?[0-9]+").map(int).desc("integer for the coefficient (make sure to not have spaces between the sign and the degree value)")
    variable = (parsy.regex(r"x") >> parsy.regex(r"[1-9][0-9]*").map(int)).desc("variable in the form 'x[1-9][0-9]*'")
    term     = parsy.seq(space >> coeff, space >> variable).map(tuple)

    if allowEq:
        equality = space >> parsy.regex(r"(=|>=)").desc("= or >=")
    else:
        equality = space >> parsy.regex(r">=").desc(">=")

    degree   = space >> parsy.regex(r"[+-]?[0-9]+").map(int).desc("integer for the degree")

    end      = parsy.regex(r";").desc("termination of rhs with ';'")
    finish   = space >> end

    return parsy.seq(term.many(), equality, degree << finish).map(tuple)


def flatten(constraintList):
    result = list()
    for oneOrTwo in constraintList:
        for c in oneOrTwo:
            result.append(c)
    return result

def getOPBParser():
    numVar = (parsy.regex(r" *#variable *= *") >> parsy.regex(r"[0-9]+")) \
                    .map(int) \
                    .desc("Number of variables in the form '#variable = [0-9]+'")
    numC = (parsy.regex(r" *#constraint *= *") >> parsy.regex(r"[0-9]+")) \
                    .map(int) \
                    .desc("Number of constraints in the form '#constraint = [0-9]+'")

    eol = parsy.regex(" *\n").desc("return at end of line")
    emptyLine = parsy.regex(r"(\s*)").desc("empty line")
    commentLine = parsy.regex(r"(\s*\*.*)").desc("comment line starting with '*'")
    header = (parsy.regex(r"\* ") >> parsy.seq(numVar, numC) << parsy.regex("\n")) \
                    .desc("header line in form of '* #variable = [0-9]+ #constraint = [0-9]+'")

    nothing = (emptyLine << eol | commentLine << eol).many()

    constraint = getOPBConstraintParser().bind(Inequality.fromParsy)

    return parsy.seq(header, (nothing >> constraint << eol).many().map(flatten)) \
     + ((emptyLine | commentLine).map(lambda x: []) | constraint.map(lambda x: [x])) << eol.optional()

class InvalidProof(Exception):
    pass

class EqualityCheckFailed(InvalidProof):
    def __init__(self, expected, got):
        self.expected = expected
        self.got = got

@register_rule
class ConstraintEquals(Rule):
    Id = "e"

    @staticmethod
    def getParser():
        space = parsy.regex(" +").desc("space")

        opb = space.optional() >> parsy.regex(r"opb") \
                >> space >> Inequality.getOPBParser(allowEq = False)
        cnf = space.optional() >> parsy.regex(r"cnf") \
                >> space >> Inequality.getCNFParser()

        constraintId = space.optional() >> parsy.regex(r"[+-]?[0-9]+ ") \
            .map(int).desc("constraintId")

        return parsy.seq(constraintId, opb | cnf).combine(ConstraintEquals.fromParsy)

    @staticmethod
    def fromParsy(constraintId, constraint):
        return ConstraintEquals(constraintId, constraint[0])

    def __init__(self, constraintId, constraint):
        self.constraintId = constraintId
        self.constraint = constraint

    def compute(self, antecedents):
        if self.constraint != antecedents[0]:
            raise EqualityCheckFailed(self.constraint, antecedents[0])

    def numConstraints(self):
        return 0

    def antecedentIDs(self):
        return [self.constraintId]

    def __eq__(self, other):
        return self.constraintId == other.constraintId \
            and self.constraint == other.constraint

    def isGoal(self):
        return True

class ContradictionCheckFailed(InvalidProof):
    pass

@register_rule
class IsContradiction(Rule):
    Id = "c"

    @staticmethod
    def getParser():
        space = parsy.regex(" +").desc("space")
        constraintId = space.optional() \
            >> parsy.regex(r"[+-]?[0-9]+") \
                .map(int).map(IsContradiction.fromParsy)
        zero  = parsy.regex("0").desc("0 to end number sequence")

        return constraintId << space << zero

    @staticmethod
    def fromParsy(constraintId):
        return IsContradiction(constraintId)

    def __init__(self, constraintId):
        self.constraintId = constraintId

    def compute(self, antecedents):
        if not antecedents[0].isContradiction():
            raise ContradictionCheckFailed()

    def numConstraints(self):
        return 0

    def antecedentIDs(self):
        return [self.constraintId]

    def __eq__(self, other):
        return self.constraintId == other.constraintId

    def isGoal(self):
        return True

@register_rule
class LinearCombination(Rule):
    Id = "l"

    @staticmethod
    def getParser(create = True):
        space = parsy.regex(" +").desc("space")
        factor = parsy.regex(r"[0-9]+").map(int).desc("factor")
        constraintId = parsy.regex(r"[1-9][0-9]*").map(int).desc("constraintId")
        zero  = parsy.regex("0").desc("0 to end number sequence")

        summand = parsy.seq(factor << space, constraintId << space).map(tuple)
        parser = space.optional() >> summand.many() << zero

        if create:
            return parser.map(LinearCombination.fromParsy)
        else:
            return parser

    @staticmethod
    def fromParsy(sequence):
        factors, antecedentIds = zip(*sequence)
        return LinearCombination(factors, antecedentIds)

    def __init__(self, factors, antecedentIds):
        assert len(factors) == len(antecedentIds)
        self.antecedentIds = antecedentIds
        self.factors = factors

    def compute(self, antecedents):
        result = Inequality()

        for factor, constraint in zip(self.factors, antecedents):
            result.addWithFactor(factor, constraint)

        return [result]

    def numConstraints(self):
        return 1

    def antecedentIDs(self):
        return self.antecedentIds

    def __eq__(self, other):
        print(self.antecedentIds)
        print(other.antecedentIds)
        return self.antecedentIds == other.antecedentIds

    def isGoal(self):
        return False

@register_rule
class Division(LinearCombination):
    Id = "d"

    @staticmethod
    def getParser():
        space = parsy.regex(" +").desc("space")
        divisor = parsy.regex(r"[1-9][0-9]*").map(int).desc("positive divisor")

        return parsy.seq(
                space.optional() >> divisor,
                LinearCombination.getParser(create = False)
            ).combine(Division.fromParsy)

    @staticmethod
    def fromParsy(divisor, sequence):
        factors, antecedentIds = zip(*sequence)
        return Division(divisor, factors, antecedentIds)

    def __init__(self, divisor, factors, antecedentIds):
        assert len(factors) == len(antecedentIds)
        super().__init__(factors, antecedentIds)
        self.divisor = divisor

    def compute(self, antecedents):
        constraint = super().compute(antecedents)[0]
        constraint.divide(self.divisor)
        return [constraint]

    def __eq__(self, other):
        return super().__eq__(other) and self.divisor == other.divisor

@register_rule
class Saturation(LinearCombination):
    Id = "s"

    @staticmethod
    def getParser():
        return LinearCombination.getParser(create = False).map(Saturation.fromParsy)

    @staticmethod
    def fromParsy(sequence):
        factors, antecedentIds = zip(*sequence)
        return Saturation(factors, antecedentIds)

    def compute(self, antecedents):
        constraint = super().compute(antecedents)[0]
        constraint.saturate()
        return [constraint]

class LoadFormula(Rule):
    id = "f"

    @staticmethod
    def getParser(formula):
        return parsy.regex(r" 0") >> parsy.success(LoadFormula(formula))

    def __init__(self, formula):
        self.formula = formula

    def compute(self, antecedents):
        return self.formula

    def numConstraints(self):
        return len(self.formula)

    def antecedentIDs(self):
        return []


class LoadFormulaWrapper():
    def __init__(self, formula):
        self.formula = formula
        self.Id = LoadFormula.id

    def getParser(self):
        if self.formula is None:
            # for memory efficincy reasons
            raise RuntimeError("Can not call load Formula twice.")

        result = LoadFormula.getParser(self.formula)
        self.formula = None
        return result


DBEntry = structclass("DBEntry","rule constraint numUsed")
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
                "skipUnused": True
            }

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

    def mapRulesToDB(self):
        constraintNum = 0

        # Forward pass to find goals
        for rule in self.rules:
            for i in range(rule.numConstraints()):
                self.db.append(DBEntry(
                    rule = rule,
                    constraint = None,
                    numUsed = 0))

                if rule.isGoal():
                    self.goals.append(constraintNum)
                constraintNum += 1

            if rule.isGoal() and rule.numConstraints() == 0:
                self.goals.extend(rule.antecedentIDs())

        self.state = Verifier.State.READ_PROOF

    def markUsed(self):
        # Backward pass to mark used rules
        while self.goals:
            goal = self.goals.pop()
            line = self.db[goal]
            line.numUsed += 1
            if line.numUsed == 1:
                for antecedent in line.rule.antecedentIDs():
                    self.goals.append(antecedent)

        self.state = Verifier.State.MARKED_LINES

    def decreaseUse(self, line):
        line.numUsed -= 1
        assert line.numUsed >= 0
        if line.numUsed == 0:
            # free space of constraints that are no longer used
            if not self.settings.disableDeletion:
                line.constraint = None

    def execRule(self, rule, numInRule = None):
        assert rule is not None
        if self._execCacheRule is not rule:
            logging.info("running rule: %s" %(rule))
            self._execCacheRule = rule
            antecedentIDs = rule.antecedentIDs()

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
                    self.execRule(rule)
            elif line.numUsed > 0 or \
                    self.settings.skipUnused == False:
                assert numInRule is not None
                line.constraint = self.execRule(rule, numInRule)
                logging.info("new line: %i: %s"%(lineNum, str(line.constraint)))
                if rule.isGoal():
                    self.decreaseUse(line)

        self.state = Verifier.State.DONE

    def checkInvariants(self):
        if self.settings.isInvariantsOn:
            pass

    def __call__(self, rules):
        self.init(rules)
        self.checkInvariants()

        self.mapRulesToDB()
        self.checkInvariants()

        self.markUsed()
        self.checkInvariants()

        self.compute()
        self.checkInvariants()

class MyParseError(parsy.ParseError):
    def __init__(self, orig, fileName):
        super().__init__(orig.expected, orig.stream, orig.index)
        self.fileName = fileName

    def line_info(self):
        lineInfo = super().line_info()
        lineInfoSplit = lineInfo.split(":")
        if len(lineInfoSplit) == 2:
            lineInfo = "%i:%i"%(int(lineInfoSplit[0]) + 1, int(lineInfoSplit[1]) + 1)
        return "%s:%s"%(self.fileName, lineInfo)


def run(formulaFile, rulesFile, settings = None):
    rules = list(registered_rules)
    try:
        formula = getOPBParser().parse(formulaFile.read())
        numVars, numConstraints = formula[0]
        constraints = formula[1]
    except parsy.ParseError as e:
        raise MyParseError(e, formulaFile.name)

    rules.append(LoadFormulaWrapper(constraints))
    try:
        rules = RuleParser().parse(rules, rulesFile.read())
    except parsy.ParseError as e:
        raise MyParseError(e, rulesFile.name)

    verify = Verifier(settings)
    verify(rules)

def runUI(*args):
    try:
        run(*args)
    except MyParseError as e:
        print(e)
        exit(1)


def main():
    p = argparse.ArgumentParser(
        description = """Command line tool to verify derivation
            graphs. See Readme.md for a description of the file
            format.""")
    p.add_argument("formula", help="Formula containing axioms.", type=argparse.FileType('r'))
    p.add_argument("derivation", help="Derivation graph.", type=argparse.FileType('r'))
    p.add_argument("-r", "--refutation", default=False, action="store_true",
        help="Fail if the derivation is not a refutation, i.e. it does not contain contradiction.")
    p.add_argument("-l", "--lazy", default=False, action="store_true",
        help="Only check steps that are necessary for contradiction.")

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

    try:
        runUI(args.formula, args.derivation, verifyerSettings)
    except Exception as e:
        if args.loglevel != logging.DEBUG:
            print("Sorry, there was an internal error. Please rerun with debugging and make a bug report.")
        else:
            raise e


if __name__ == '__main__':
    main()