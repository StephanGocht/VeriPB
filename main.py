import argparse
from collections import namedtuple
from recordclass import structclass
#todo recordclass requires install
from math import copysign, ceil

Term = structclass("Term","coefficient variable")

class Inequality():
    """
    Constraint representing sum of terms greater or equal degree.
    Terms are stored in normalized form, i.e. negated literals but no negated coefficient.
    The sign of the literal is stored in the coefficient
    """

    def __init__(self, terms = list(), degree = 0):
        self.degree = degree
        self.terms = sorted(terms, key = lambda x: abs(x.variable))

    def addWithFactor(self, factor, other):
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
                self.degree -= cancellation
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
    def clean(self):
        self.db = list()
        self.db.append(DBEntry(DummyRule(), None, 0))
        self.goals = list()

    def forwardScan(self, rules):
        # starting index is 1
        constraintNum = 1

        # Forward pass to find goals
        for rule in rules:
            for i in range(rule.numConstraints()):
                self.db.append(DBEntry(
                    rule = rule,
                    constraint = None,
                    numUsed = 0))

                if rule.isGoal():
                    self.goals.append(constraintNum)
                constraintNum += 1

    def markUsed(self):
        # Backward pass to mark used rules
        while self.goals:
            goal = self.goals.pop()
            for antecedent in self.db[goal].rule.antecedentIDs():
                assert antecedent < goal
                db[antecedent].numUsed += 1
                if db[antecedent].numUsed == 1:
                    goals.append(antecedent)

    def compute(self):
        # Forward pass to compute constraitns
        rule = None
        constraints = None
        antecedentIDs = None

        for entry in self.db:
            if entry.numUsed > 0 or entry.rule.isGoal:
                if rule is None or rule != entry.rule:
                    assert(constraints is None or len(constraints) == 0)

                    rule = entry.rule
                    antecedentIDs = entry.rule.antecedentIDs()

                    constraints = entry.rule.computeConstraints(
                        [db[i].constraint for i in antecedentIDs])
                    constraints.reverse()

                    for i in antecedentIDs:
                        db[i].numUsed -= 1
                        assert db[i].numUsed >= 0
                        if db[i].numUsed == 0:
                            # free space of constraints that are no longer used
                            db[i].constraint = None

                assert(len(constraints) > 0)

                entry.constraint = constraints.pop()

    def __call__(self, rules):
        self.clean()
        self.forwardScan(rules)
        self.markUsed()
        self.compute()

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