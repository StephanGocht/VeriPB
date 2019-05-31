from refpy.constraints import Inequality

import parsy

from refpy import InvalidProof

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
    Id = "a"

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

class LoadLitteralAxioms(Rule):
    id = "l"

    @staticmethod
    def getParser(formula):
        return parsy.regex(r" *[1-9][0-9]*") \
                .map(int)\
                .map(LoadLitteralAxioms.fromParsy)\
                .desc("number of literals") << parsy.regex(r" 0")

    @staticmethod
    def fromParsy(numLiterals):
        return LoadLitteralAxioms(numLiterals)

    def __init__(self, numLiterals):
        self.numLiterals = numLiterals

    def compute(self, antecedents):
        result = list()
        for i in range(self.numLiterals):
            result.append(Inequality([Term(1, i)], 1))
            result.append(Inequality([Term(1,-i)], 1))

        return result

    def numConstraints(self):
        return 2 * self.numLiterals

    def antecedentIDs(self):
        return []



class LoadFormula(Rule):
    id = "f"

    @staticmethod
    def getParser(formula):
        def f(numVars):
            return LoadFormula(formula, numVars)


        return parsy.regex(r" *[1-9][0-9]*") \
                .map(int)\
                .map(LoadLitteralAxioms.fromParsy)\
                .desc("number of constraints")\
                .optional().map(f) \
                << parsy.regex(r" 0")

    def __init__(self, formula, numVars = None):
        assert numVars is None or len(formula) == numVars

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