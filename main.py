import argparse
from collections import namedtuple
from recordclass import structclass
#todo recordclass requires install
from math import copysign, ceil
from enum import Enum

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

    def __init__(self, terms = list(), degree = 0):
        self.degree = degree
        self.terms = sorted(terms, key = lambda x: abs(x.variable))

    def addWithFactor(self, factor, other, variableUpperBounds = AllBooleanUpperBound()):
        self.degree += factor * other.degree
        result = list()

        otherTerms = map(lambda x: Term(factor * x.coefficient, x.variable), other.terms)
        myTerms = iter(self.terms)

        other = next(otherTerms, None)
        my    = next(myTerms, None)
        while (other is not None or my is not None):
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
                self.degree -= cancellation * variableUpperBounds[abs(my.variable)]
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
        return self.degree == other.degree \
            and self.terms == other.terms

    def __str__(self):
        return " ".join(
            ["%+ix%i"%(term.coefficient, term.variable)
                for term in self.terms]) + \
            " >= %i" % self.degree

    def __repr__(self):
        return str(self)


class OpbParsingException(Exception):
    pass

class OpbParser:
    def cbHeader(self, var, constr):
        pass

    def cbTerm(self, coeff, var):
        pass

    def cbDegree(self, op, degree):
        pass

    def parseOpbFile(self, fileName):
        emptyLine = re.compile(r"(^ *\*)|(^\s*$)")
        header = re.compile(r"\* *#variable *= *(?P<vars>[0-9]+) *#constraint *= *(?P<constr>[0-9]+)")
        term = re.compile(r" *(?P<coeff>[+-]?[0-9]+) *x(?P<var>[0-9]+) *")
        degree = re.compile(r"(?P<op>(>=)|=) *(?P<degree>[+-]?[0-9]+) *;")

        foundHeader = False
        with open(fileName, "r") as f:
            for lineNum, line in enumerate(f):
                match = emptyLine.match(line)
                if (match):
                    if (not foundHeader):
                        match = header.match(line)
                        if match:
                            foundHeader = True
                            if self.cbHeader is not None:
                                self.cbHeader(int(match["vars"]), int(match["constr"]))
                else:
                    start = 0
                    for m in term.finditer(line):
                        if (start != m.start()):
                            raise OpbParsingException("Unexpected Symbol", fileName, lineNum, start)
                        start = m.end()
                        if self.cbTerm is not None:
                            self.cbTerm(int(m["coeff"]), int(m["var"]))
                    m = degree.match(line, start)
                    if m is None or start != m.start():
                        raise OpbParsingException("Unexpected Symbol", fileName, lineNum, start)
                    if self.cbDegree is not None:
                        self.cbDegree(m["op"], int(m["degree"]))
        if (not foundHeader):
            raise OpbParsingException("No header found. (* #variables = ... #constraints = ...) in %s"%(fileName))

registered_rules = []
def register_rule(rule):
    registered_rules.append(rule)

class Rule():
    id = None

    def computeConstraints(self, antecedents):
        """
        Args:
            antecedents (list): Antecedents in the order given by antecedentIDs

        Returns:
            computed constraint
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
        False

class DummyRule(Rule):
    def computeConstraints(self, db):
        return [Inequality([], 0)]

    def numConstraints(self):
        return 1

    def antecedentIDs(self):
        return []

class RuleFactory():
    def __init__(self, rules, file):
        self.rules = dict()
        for rule in rules:
            if rule.id not in rules.keys():
                assert len(rule.id) == 1
                self.rules[rule.id] = rule
            else:
                raise ValueError("Duplicate key for rule '%s'" % (rule.id))

        self._file = file

        header = self._file.readline()
        header = header.split("using")
        assert len(header) == 2
        headerText = header[0]
        usedRules = header[1].split(" ")
        assert usedRules[-1] == "0"
        for rule in usedRules[:-1]:
            if rule not in rules.keys():
                raise NotImplementedError("Unsupported rule '%s'" % (rule))

    def __iter__(self):
        for line in self._file:
            yield rules[line[0]](line)

class SimpleRule(Rule):
    """
    A simple rule is a rule that is starting with an identifier followed
    by a stream of numbers that is 0 terminated.
    """
    def __init__(self, stream):
        self._line = stream

    @property
    def id(self):
        return self._id

    def numberStream(self):
        words = self._line.split(" ")
        assert self._id == words[0]
        foundEnd = False
        for (i,word) in enumerate(words)[1:]:
            assert foundEnd == False

            res = int(word)
            if res != 0:
                yield res
            else:
                foundEnd = True

class LoadFormula(SimpleRule):
    id = "f"

    def __init__(self, stream, formula):
        super().__init__(stream)
        self._formula = formula

    def __call__(self, db):
        yield

class LoadFormulaWrapper():
    id = LoadFormula.id

    def __init__(self, formula):
        self.formula = formula

    def __call__(self, stream):
        return LoadFormula(stream, self.formula)


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
            self.rules = iter(verifier.rules)
            self.counter = 0
            self.rule = None
            self.lines = iter(verifier.db)

        def __iter__(self):
            return self

        def __next__(self):
            if self.counter == 0:
                self.rule = next(self.rules)
                self.counter = self.rule.numConstraints()
            if self.counter != 0:
                self.counter -= 1
                line = next(self.lines)
            else:
                line = None
            return (
                self.rule,
                line,
                None if line is None else self.rule.numConstraints() - (self.counter + 1)
            )

    class Settings():
        def __init__(self, preset = None):
            self.isInvariantsOn  = False
            self.disableDeletion = False
            self.skipUnused      = True

            if preset is not None:
                self.setPreset(preset)

        def setPreset(self, preset):
            for key, value in preset.items():
                if hasattr(self, key):
                    setattr(self, key, value)
                else:
                    raise ValueError("Got unknown setting %s."%key)



    def __init__(self, settings = Settings()):
        self.settings = settings

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
            self._execCacheRule = rule
            antecedentIDs = rule.antecedentIDs()

            self._execCache = rule.computeConstraints(
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
            if line is None:
                if rule.isGoal():
                    self.execRule(rule)
            elif line.numUsed > 0 or \
                    self.settings.skipUnused == False:
                assert numInRule is not None
                line.constraint = self.execRule(rule, numInRule)
                if rule.isGoal():
                    self.decreaseUse(line)

        self.state = Verifier.State.DONE

    def checkInvariants(self):
        if self.settings.isInvariantsOn:
            if self.state >= Verifier.State.READ_PROOF:
                for goal in self.goals:
                    assert db[goal].rule.isGoal() == True

    def __call__(self, rules):
        self.init(rules)
        self.checkInvariants()

        self.mapRulesToDB()
        self.checkInvariants()

        self.markUsed()
        self.checkInvariants()

        self.compute()
        self.checkInvariants()

def main():
    p = argparse.ArgumentParser(
        descritpion = """Command line tool to verify derivation
            graphs. See Readme.md for a description of the file
            format.""")

    p.add_argumet("formula", help="Formula containing axioms.")
    p.add_argumet("derivation", help="Derivation graph.")
    p.add_argumet("-r", "--refutation", type=bool, default=False, action="store_true",
        help="Fail if the derivation is not a refutation, i.e. it does not contain contradiction.")
    p.add_argumet("-l", "--lazy", type=bool, default=False, action="store_true",
        help="Only check steps that are necessary for contradiction.")

    rules = registered_rules
    rules.append(LoadFormulaWrapper(args.formula))
    ruleFactory = RuleFactory(rules, args.derivation)
    rules = list(ruleFactory)

if __name__ == '__main__':
    main()