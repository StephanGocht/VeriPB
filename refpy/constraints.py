from recordclass import structclass
import parsy

def copysign(a, b):
       if b >= 0:
               return abs(a)
       else:
               return -abs(a)

from refpy.parser import getOPBConstraintParser, getCNFConstraintParser

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

            result.append(Inequality([Term(a,x) for a,x in terms], degree))
            if eq == "=":
                result.append(Inequality([Term(-a,x) for a,x in terms], -degree))

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
            term.coefficient = (term.coefficient + d - 1) // d
        self.degree = (self.degree + d - 1) // d

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