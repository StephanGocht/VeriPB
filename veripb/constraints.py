from veripb.optimized.constraints import CppInequality
from veripb.optimized.parsing import VariableNameManager, parseConstraintOpb
from veripb.exceptions import ParseError


def copysign(a, b):
    if b >= 0:
        return abs(a)
    else:
        return -abs(a)

class Term:
    def __init__(self, coeff, var):
        self.coefficient = coeff
        self.variable = var

    def __iter__(self):
        return iter((self.coefficient, self.variable))

    def __eq__(self, other):
        return self.coefficient == other.coefficient \
            and self.variable == other.variable


class AllBooleanUpperBound():
    """
    Stub that needs to be replaced if we want to start handeling
    integer constraitns.
    """
    def __getitem__(self, key):
        return 1

class LazyInequality():
    """
    Essentially the same interface as PyInequality but postpones some
    of the operations.
    """

    def __init__(self, constraint):
        self.constraint = constraint
        self.operations = []
        self.degrees = []

    @property
    def terms(self):
        return filter(
            lambda x: x.coefficient > 0,
            map(self.applyTerm, self.constraint.terms))

    @property
    def degree(self):
        return self.apply(self.constraint.degree)

    def applyTerm(self, value):
        return Term(self.apply(value.coefficient), value.variable)

    def apply(self, value):
        for op, i in self.operations:
            if op == "s":
                value = min(self.degrees[i], value)
            elif op == "d":
                value = (value + i - 1) // i
            elif op == "*":
                value *= i
        return value

    def saturate(self):
        self.degrees.append(self.apply(self.constraint.degree))
        self.operations.append(("s",len(self.degrees) - 1))
        return self

    def divide(self, d):
        self.operations.append(("d",d))
        return self

    def multiply(self, f):
        self.operations.append(("*",f))
        return self

    def add(self, other):
        result = PyInequality(self.terms, self.degree)
        return result.add(other)

    def resolve(self, other, resolvedVar):
        result = PyInequality(self.terms, self.degree)
        return result.resolve(other, resolvedVar)

    def contract(self):
        pass

    def negated(self):
        result = PyInequality(self.terms, self.degree)
        return result.negated()

    def copy(self):
        return LazyInequality(self)

    def __eq__(self, other):
        return PyInequality(self.terms, self.degree) == other

    def __repr__(self):
        return str(PyInequality(self.terms, self.degree))

    def toString(self, varNameMapping):
        return PyInequality(self.terms, self.degree).toString(varNameMapping)

    def isContradiction(self):
        return PyInequality(self.terms, self.degree).isContradiction()

    def implies(self, other):
        return PyInequality(self.terms, self.degree).implies(other)

class PyInequality():
    """
    Constraint representing sum of terms greater or equal degree.
    Terms are stored in normalized form, i.e. negated literals but no
    negated coefficient. Variables are represented as ingtegers
    greater 0, the sign of the literal is stored in the sign of the
    integer representing the variable, i.e. x is represented as 2 then
    (not x) is represented as (-2).

    For integers 0 <= x <= d the not x is defined as d - x. Note that
    a change in the upperbound invalidates the stored constraint.
    """

    def __init__(self, terms = list(), degree = 0, variableUpperBounds = AllBooleanUpperBound()):
        self.degree = degree
        self._terms = list(terms)
        self._dict = None
        self.expanded = False
        self.variableUpperBounds = variableUpperBounds
        self.normalize()


    @property
    def dict(self):
        self._expand()
        return self._dict

    @property
    def terms(self):
        self.contract()
        return self._terms

    @terms.setter
    def terms(self, l):
        self.contract()
        self._terms = l

    def contract(self):
        assert((self._dict is None) != (self._terms is None))
        if self._terms is None:
            self._terms = [x for x in self.dict.values() if x.coefficient != 0]
            self._dict = None

    def _expand(self):
        assert((self._dict is None) != (self._terms is None))
        if self._dict is None:
            self._dict = {abs(x.variable): x for x in self._terms}
            self._terms = None

    def normalize(self):
        occurs = set()
        for term in self.terms:
            if abs(term.variable) in occurs:
                raise ValueError("Variable occurs twice, currently not supported.")
            else:
                occurs.add(abs(term.variable))

            if term.coefficient < 0:
                term.variable = -term.variable
                term.coefficient = abs(term.coefficient)
                self.degree += self.variableUpperBounds[abs(term.variable)] * term.coefficient

        # self.terms.sort(key = lambda x: abs(x.variable))

    # @profile
    def add(self, other):
        self.degree += other.degree

        for other in other.terms:
            try:
                my = self.dict[abs(other.variable)]
            except KeyError:
                self.terms.append(Term(other.coefficient, other.variable))
                self.dict[abs(other.variable)] = self.terms[-1]
            else:
                if (abs(my.variable) == abs(other.variable)):
                    a = copysign(my.coefficient, my.variable)
                    b = copysign(other.coefficient, other.variable)
                    newCoefficient = a + b
                    newVariable = copysign(abs(my.variable), newCoefficient)
                    newCoefficient = abs(newCoefficient)
                    cancellation = max(0, max(my.coefficient, other.coefficient) - newCoefficient)
                    self.degree -= cancellation * self.variableUpperBounds[abs(my.variable)]

                    my.coefficient = newCoefficient
                    my.variable = newVariable

        return self

    def saturate(self):
        for term in self.terms:
            term.coefficient = min(
                term.coefficient,
                self.degree)
        self.terms = [x for x in self.terms if x.coefficient > 0]
        return self

    def divide(self, d):
        for term in self.terms:
            term.coefficient = (term.coefficient + d - 1) // d
        self.degree = (self.degree + d - 1) // d
        return self

    def multiply(self, f):
        for term in self.terms:
            term.coefficient = term.coefficient * f
        self.degree = self.degree * f
        return self

    def resolve(self, other, resolvedVar):
        # todo: clean up. this is ugly
        # we do not know if we have a lazy PyInequality or not
        other = PyInequality(other.terms, other.degree)

        if self.degree != 1 or other.degree != 1:
            # todo: find a good implementation for this case
            raise NotImplementedError("Resolution is currently only implemented for clausal constraints (degree = 1)")

        resolvedVar = abs(resolvedVar)
        mine   =  self.dict.get(resolvedVar, None)
        theirs = other.dict.get(resolvedVar, None)

        if mine is None:
            return self
        elif theirs is None:
            return other
        else:
            if mine.coefficient != 1 or theirs.coefficient != 1:
                raise NotImplementedError("Resolution is currently only implemented for clausal constraints (all coefficients must be 1)")

            if mine.variable * theirs.variable >= 0:
                raise NotImplementedError("Resolution is currently only implemented for clashing constraints (one must contain ~resolvedVar and the other resolvedVar)")

            return self.add(other).saturate()

    def isContradiction(self):
        slack = -self.degree
        for term in self.terms:
            slack += term.coefficient
        return slack < 0

    def implies(self, other):
        """
        check if self semantically implies other
        """

        weakenCost = 0
        for var, mine in self.dict.items():
            theirs = other.dict.get(var, Term(0, mine.variable))
            if mine.variable != theirs.variable:
                weakenCost += mine.coefficient
            elif mine.coefficient > theirs.coefficient:
                weakenCost += mine.coefficient - theirs.coefficient

        if self.degree - weakenCost < other.degree:
            return False
        else:
            return True

    def negated(self):
        self.degree = -self.degree + 1
        for term in self.terms:
            self.degree += term.coefficient
            term.variable = -term.variable

        return self

    def __eq__(self, other):
        # note that terms is assumed to be soreted
        key = lambda x: abs(x.variable)
        return self.degree == other.degree \
            and sorted(self.terms, key = key) == sorted(other.terms, key = key)

    def toOPB(self, varNameMapping = None):
        if varNameMapping == None:
            varNameMapping = lambda x: "x%i"%(x)

        def term2str(term):
            if term.variable < 0:
                return "%+i ~%s"%(term.coefficient, varNameMapping(-term.variable))
            else:
                return "%+i %s"%(term.coefficient, varNameMapping(term.variable))

        return " ".join(
            map(term2str, self.terms)) + \
            " >= %i" % self.degree

    def __str__(self):
        return self.toOPB()

    def toString(self, varNameMapping):
        return self.toOPB(varNameMapping)

    def __repr__(self):
        return str(self)

    def copy(self):
        return LazyInequality(self)

class IneqFactory():
    def __init__(self, _freeNames = True):
        self.freeNames = _freeNames
        self.varNameMgr = VariableNameManager(self.freeNames)

    def litAxiom(self, lit):
        return PyInequality([Term(1, lit)], 0)

    def fromTerms(self, terms, degree):
        return PyInequality([Term(a,l) for a,l in terms], degree)

    def isLit(self, lit):
        return ((lit[0] == "~") and self.isVarName(lit[1:])) \
            or self.isVarName(lit)

    def lit2int(self, lit):
        return self.varNameMgr.getLit(lit)

    def int2lit(self, lit):
        res = ""
        if lit < 0:
            res = "~"
            lit = -lit
        res += self.num2Name(lit)
        return res

    def intlit2int(self, lit):
        var = self.name2Num("x%i"%(abs(lit)))
        if lit < 0:
            return -var
        else:
            return var

    def isVarName(self, name):
        assert(name == name.strip())

        if not self.freeNames:
            if name[0] != "x":
                return False
            else:
                return True

        if len(name) >= 2 \
                and (ord(name[0]) in range(ord("A"), ord("Z") + 1) \
                    or ord(name[0]) in range(ord("a"), ord("z") + 1)) \
                and ";" not in name and "=" not in name:
            return True
        else:
            return False

    def name2Num(self, name):
        return self.varNameMgr.getVar(name)

    def num2Name(self, num):
        return self.varNameMgr.getName(num)

    def numVars(self):
        return self.varNameMgr.maxVar()

    def toString(self, constraint):
        def f(num):
            return self.num2Name(num)

        return constraint.toString(f)

def terms2lists(terms):
    return zip(*[(a,l) for a,l in terms]) \
        if len(terms) > 0 else ([],[])

class PropEngine:
    def attach(self, ineq):
        pass

    def detach(self, ineq):
        pass

    def attachTmp(self, ineq):
        raise NotImplementedError("arbitrary precision unit propagation")

    def reset(self, ineq):
        pass

    def checkSat(self, v):
        raise NotImplementedError("arbitrary precision solution check")

    def printStats(self):
        pass

class CppIneqFactory(IneqFactory):
    def __init__(self, enableFreeNames = True):
        super().__init__(enableFreeNames)

    def litAxiom(self, lit):
        return CppInequality([1], [lit], 0)

    def fromTerms(self, terms, degree):
        coefs, lits = terms2lists(terms)
        coefs = list(coefs)
        lits  = list(lits)
        return CppInequality(coefs, lits, degree)

    def parse(self, wordIter, allowMultiple = False):
        result = parseConstraintOpb(self.varNameMgr, wordIter)
        if allowMultiple:
            return result
        else:
            return result[0]


# class BigIntIneqFactory(IneqFactory):
#     def __init__(self, enableFreeNames = True):
#         super().__init__(enableFreeNames)

#     def litAxiom(self, lit):
#         return CppInequalityBigInt([1], [lit], 0)

#     def fromTerms(self, terms, degree):
#         coefs, lits = terms2lists(terms)
#         coefs = list(coefs)
#         lits  = list(lits)
#         return CppInequalityBigInt(coefs, lits, degree)

#     def parse(self, wordIter, allowMultiple = False):
#         result = parseConstraintOpbBigInt(self.varNameMgr, wordIter)
#         if allowMultiple:
#             return result
#         else:
#             if (result[1] != None):
#                 raise ParseError("Equality not allowed here.")
#             return result[0]

