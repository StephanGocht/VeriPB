import logging

import veripb.constraints
from veripb.constraints import Term
from veripb.parser import OPBParser, WordParser
from veripb.timed_function import TimedFunction
from veripb.rules_register import register_rule

from veripb import InvalidProof

import itertools

class Rule():
    @staticmethod
    def getParser(context):
        """
        Returns:
            A parser that creates this rule from the string following the Id
        """
        raise NotImplementedError()

    @classmethod
    def parse(cls, line, context):
        return cls.getParser(context).parse(line.rstrip())

    def compute(self, antecedents, context = None):
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

    def deleteConstraints(self):
        """
        Return a list of constraintId's that can be removed from the database
        """
        return []

    def allowedRules(self, context, currentRules):
        return currentRules

    def __str__(self):
        return type(self).__name__

class EmptyRule(Rule):
    def compute(self, antecednets, context):
        return []

    def numConstraints(self):
        return 0

    def antecedentIDs(self):
        return []

class DummyRule(Rule):
    def compute(self, antecednets, context):
        return [context.ineqFactory.fromTerms([], 0)]

    def numConstraints(self):
        return 1

    def antecedentIDs(self):
        return []



class EqualityCheckFailed(InvalidProof):
    def __init__(self, expected, got):
        self.expected = expected
        self.got = got

class ReverseUnitPropagationFailed(InvalidProof):
    pass

@register_rule
class DeleteConstraints(Rule):
    Id = "d" #(d)elete

    @classmethod
    def parse(cls, line, context):
        with WordParser(line) as words:
            which = list(map(int, words))

            if (which[-1] == 0):
                which = which[:-1]

            if 0 in which:
                raise ValueError("Can not delete constraint with index 0.")

        return cls(which)

    def __init__(self, toDelete):
        self.toDelete = toDelete

    def compute(self, antecedents, context = None):
        return []

    def numConstraints(self):
        return 0

    def antecedentIDs(self):
        return []

    def isGoal(self):
        return False

    def deleteConstraints(self):
        return self.toDelete

@register_rule
class Assumption(Rule):
    Id = "a"

    @classmethod
    def parse(cls, line, context):
        with WordParser(line) as words:
            parser = OPBParser(
                    ineqFactory = context.ineqFactory,
                    allowEq = False)
            ineq = parser.parseConstraint(words)

        return cls(ineq[0])

    def __init__(self, constraint):
        self.constraint = constraint

    def numConstraints(self):
        return 1

    def antecedentIDs(self):
        return []

    def __eq__(self, other):
        return self.constraint == other.constraint and self.Id == other.Id

    def isGoal(self):
        return False

    @TimedFunction.time("Assumption.compute")
    def compute(self, antecedents, context):
        context.usesAssumptions = True
        return [self.constraint]


@register_rule
class ReverseUnitPropagation(Rule):
    Id = "u"

    @classmethod
    def parse(cls, line, context):
        with WordParser(line) as words:
            peek = next(words)
            w = list()

            if peek == "w":
                if getattr(context, "foundLoadFormula", False) == False:
                    raise ValueError("You are not allowed to use redundancy checks"\
                        "before the formula is loaded.")

                if getattr(context, "objective", None) is not None:
                    raise ValueError("You are not allowed to use redundancy checks"\
                        "in optimization problems.")

                for nxt in words:
                    if nxt in ["0", ";"]:
                        break
                    w.append(context.ineqFactory.lit2int(nxt))
            else:
                words = itertools.chain([peek], words)

            parser = OPBParser(
                    ineqFactory = context.ineqFactory,
                    allowEq = False)
            ineq = parser.parseConstraint(words)

        return cls(ineq[0], w, context.ineqFactory.numVars())

    def __init__(self, constraint, w, numVars):
        self.constraint = constraint
        self.w = w
        self.numVars = numVars

    def numConstraints(self):
        return 1

    def antecedentIDs(self):
        return "all"

    def __eq__(self, other):
        return self.constraint == other.constraint

    def isGoal(self):
        return False

    @TimedFunction.time("ReverseUnitPropagation.compute")
    def compute(self, antecedents, context):
        context.propEngine.increaseNumVarsTo(self.numVars)
        success = self.constraint.ratCheck(self.w, context.propEngine)

        if success:
            return [self.constraint]
        else:
            raise ReverseUnitPropagationFailed(
                "Failed to show '%s' by reverse unit propagation."%(
                    context.ineqFactory.toString(self.constraint)))

class CompareToConstraint(Rule):
    @classmethod
    def parse(cls, line, context = None):
        with WordParser(line) as words:
            which = words.nextInt()

            parser = OPBParser(
                    ineqFactory = context.ineqFactory,
                    allowEq = False)
            ineq = parser.parseConstraint(words)
            words.expectEnd()

        return cls(which, ineq[0])

    def __init__(self, constraintId, constraint):
        self.constraintId = constraintId
        self.constraint = constraint

    def numConstraints(self):
        return 0

    def antecedentIDs(self):
        return [self.constraintId]

    def __eq__(self, other):
        return self.constraintId == other.constraintId \
            and self.constraint == other.constraint

    def isGoal(self):
        return True

@register_rule
class ConstraintEquals(CompareToConstraint):
    Id = "e"

    @TimedFunction.time("ConstraintEquals.compute")
    def compute(self, antecedents, context = None):
        antecedents = list(antecedents)
        if self.constraint != antecedents[0]:
            raise EqualityCheckFailed(self.constraint, antecedents[0])

        return []

class ImpliesCheckFailed(InvalidProof):
    def __init__(self, expected, got):
        self.expected = expected
        self.got = got

@register_rule
class ConstraintImplies(CompareToConstraint):
    Id = "i"

    def compute(self, antecedents, context = None):
        antecedents = list(antecedents)
        if not antecedents[0].implies(self.constraint):
            raise ImpliesCheckFailed(self.constraint, antecedents[0])
        return []

@register_rule
class ConstraintImpliesGetImplied(ConstraintImplies):
    Id = "j"

    @TimedFunction.time("ConstraintImpliesGetImplied.compute")
    def compute(self, antecedents, context = None):
        super().compute(antecedents)
        return [self.constraint]

    def numConstraints(self):
        return 1


class ContradictionCheckFailed(InvalidProof):
    def __str__(self):
        result = "Constraint is not a contradiction. "
        result += super().__str__()
        return result

class SolutionCheckFailed(InvalidProof):
    def __str__(self):
        result = "Provided assignment is contradicting or does not propagate to solution. "
        result += super().__str__()
        return result

@register_rule
class Solution(Rule):
    Id = "v"

    @classmethod
    def parse(cls, line, context):
        def lit2int(name):
            if name[0] == "~":
                return -context.ineqFactory.name2Num(name[1:])
            else:
                return context.ineqFactory.name2Num(name)

        with WordParser(line) as words:
            result = list(map(lit2int, words))

        return cls(result)

    def __init__(self, partialAssignment):
        self.partialAssignment = partialAssignment

    @TimedFunction.time("Solution.compute")
    def compute(self, antecedents, context):
        if not context.propEngine.checkSat(self.partialAssignment):
            raise SolutionCheckFailed()

        return [context.ineqFactory.fromTerms([Term(1, -lit) for lit in self.partialAssignment], 1)]

    def numConstraints(self):
        return 1

    def antecedentIDs(self):
        return []

    def isGoal(self):
        return True

    def deleteConstraints(self):
        return []

class ObjectiveNotFullyAssigned(InvalidProof):
    def __str__(self):
        result = "Provided assignment doesn't assign all variables in the objective. "
        result += super().__str__()
        return result

@register_rule
class ObjectiveBound(Rule):
    Id = "o"

    @classmethod
    def parse(cls, line, context):
        def lit2int(name):
            if name[0] == "~":
                return -context.ineqFactory.name2Num(name[1:])
            else:
                return context.ineqFactory.name2Num(name)

        with WordParser(line) as words:
            result = list(map(lit2int, words))

        return cls(result)

    def __init__(self, partialAssignment):
        self.partialAssignment = partialAssignment

    @TimedFunction.time("ObjectiveBound.compute")
    def compute(self, antecedents, context):
        if not context.propEngine.checkSat(self.partialAssignment):
            raise SolutionCheckFailed()

        objValue = 0
        numFoundValues = 0
        for lit in self.partialAssignment:
            if abs(lit) in context.objective:
                numFoundValues += 1
                if lit > 0:
                    objValue += context.objective[lit]

        if len(context.objective) != numFoundValues:
            raise ObjectiveNotFullyAssigned()

        # obj <= objvalue - 1
        lowerBound = context.ineqFactory.fromTerms(
            [Term(-coeff, lit) for lit, coeff in context.objective.items()],
            -(objValue - 1))

        return [lowerBound]

    def numConstraints(self):
        return 1

    def antecedentIDs(self):
        return []

    def isGoal(self):
        return True

    def deleteConstraints(self):
        return []


@register_rule
class IsContradiction(Rule):
    Id = "c"

    @classmethod
    def parse(cls, line, context):
        with WordParser(line) as words:
            which = list(map(int, words))

            if (which[-1] == 0):
                which = which[:-1]

            if len(which) != 1:
                raise ValueError("Expected exactly one constraintId.")

        return cls(which[0])

    def __init__(self, constraintId):
        self.constraintId = constraintId

    @TimedFunction.time("IsContradiction.compute")
    def compute(self, antecedents, context = None):
        antecedents = list(antecedents)
        if not antecedents[0].isContradiction():
            raise ContradictionCheckFailed()
        else:
            context.containsContradiction = True
        return []

    def numConstraints(self):
        return 0

    def antecedentIDs(self):
        return [self.constraintId]

    def __eq__(self, other):
        return self.constraintId == other.constraintId

    def isGoal(self):
        return True

@register_rule
class ReversePolishNotation(Rule):
    Id = "p"

    @classmethod
    def parse(cls, line, context):
        stackSize = 0
        def f(word):
            nonlocal stackSize

            if word in ["+", "*", "d", "w"]:
                stackSize -= 1
            elif word == "r":
                stackSize -= 2
            elif word in ["s", ";"]:
                stackSize += 0
            else:
                if context.ineqFactory.isLit(word):
                    lit = context.ineqFactory.lit2int(word)
                    word = ("l", lit)
                    stackSize += 1
                else:
                    try:
                        word = int(word)
                        stackSize += 1
                    except ValueError:
                        raise ValueError("Expected integer, literal or one of +, *, d, s, r.")
                    # else:
                    #     if word == 0:
                    #         raise ValueError("Got 0, which should only be used to terminate sequence.")

            if stackSize <= 0:
                raise ValueError("Trying to pop from empty stack in reverse polish notation.")

            return word

        with WordParser(line) as words:
            sequence = list(map(f, words))

            if sequence[-1] == 0:
                sequence.pop()
                stackSize -= 1
            if sequence[-1] == ";":
                sequence.pop()

            if stackSize != 1:
                raise ValueError("Stack should contain exactly one element at end of polish notation.");
        return cls(sequence)

    class AntecedentIterator():
        """
        Iterator that only returns the contained constraint numbers
        (antecedents) and skips everything else.
        """
        def __init__(self, instructions):
            self.instructions = iter(instructions)

        def __iter__(self):
            return self

        def __next__(self):
            while (True):
                current = next(self.instructions)
                if isinstance(current, int):
                    return current
                if current in ["*", "d", "r", "w"]:
                    # consume one more, remember that we swaped the right operand and operator
                    next(self.instructions)

    def __init__(self, instructions):
        for i, x in enumerate(instructions):
            # the last operand of multiplication and division always
            # needs to be a constant and not a constraint, so we (can) switch
            # positions, which makes it easier to distinguish constraints from
            # constants later on
            if x in ["*", "d", "r", "w"]:
                instructions[i] = instructions[i - 1]
                instructions[i - 1] = x

        self.instructions = instructions

    @TimedFunction.time("ReversePolishNotation.compute")
    def compute(self, antecedents, context = None):
        antecedents = list(antecedents)
        stack = list()
        antecedentIt = iter(antecedents)

        it = iter(self.instructions)
        ins = next(it, None)
        while ins is not None:
            if isinstance(ins, int):
                stack.append(next(antecedentIt).copy())
            if isinstance(ins, tuple):
                what = ins[0]
                if what == "l":
                    lit = ins[1]
                    stack.append(context.ineqFactory.litAxiom(lit))
                else:
                    assert(False)
            elif ins == "+":
                second = stack.pop()
                first  = stack.pop()
                stack.append(first.add(second))
            elif ins == "*":
                constraint = stack.pop()
                factor = next(it)
                stack.append(constraint.multiply(factor))
            elif ins == "d":
                constraint = stack.pop()
                divisor = next(it)
                stack.append(constraint.divide(divisor))
            elif ins == "r":
                second = stack.pop()
                first = stack.pop()
                resolvedVar = next(it)
                stack.append(first.resolve(second, resolvedVar))
            elif ins == "s":
                constraint = stack.pop()
                stack.append(constraint.saturate())
            elif ins == "w":
                nxt = next(it, None)
                assert(nxt[0] == "l")
                lit = nxt[1]
                if lit < 0:
                    logging.warn("Weakening step ignores sign of literals.")
                    lit = abs(lit)
                constraint = stack.pop()
                stack.append(constraint.weaken(lit))


            ins = next(it, None)

        assert len(stack) == 1
        stack[0].contract()
        return stack

    def numConstraints(self):
        return 1

    def antecedentIDs(self):
        return ReversePolishNotation.AntecedentIterator(self.instructions)

@register_rule
class LoadFormula(Rule):
    Id = "f"

    @classmethod
    def parse(cls, line, context):
        context.foundLoadFormula = True
        numConstraints = len(context.formula)
        with WordParser(line) as words:
            try:
                num = int(next(words))
            except StopIteration:
                num = 0
            if num == 0:
                return cls(numConstraints)
            else:
                if num != numConstraints:
                    raise ValueError(
                        "Number of constraints does not match, got %i but "\
                        "there are %i constraints."%(num, numConstraints))

                return cls(numConstraints)

    def __init__(self, numConstraints):
        self._numConstraints = numConstraints

    @TimedFunction.time("LoadFormula.compute")
    def compute(self, antecedents, context = None):
        if (len(context.formula) != self._numConstraints):
            raise InvalidProof("Wrong number of constraints")
        return context.formula

    def numConstraints(self):
        return self._numConstraints

    def antecedentIDs(self):
        return []

@register_rule
class LoadAxiom(Rule):
    Id = "l"

    @classmethod
    def parse(cls, line, context):
        with WordParser(line) as words:
            num = words.nextInt()
            words.expectEnd()
            return cls(num)

    def __init__(self, axiomId):
        self._axiomId = axiomId

    @TimedFunction.time("LoadAxiom.compute")
    def compute(self, antecedents, context):
        try:
            return [context.formula[self._axiomId - 1]]
        except IndexError as e:
            raise InvalidProof("Trying to load non existing axiom.")


    def numConstraints(self):
        return 1

    def antecedentIDs(self):
        return []

class LevelStack():
    @classmethod
    def setup(cls, context, namespace = None):

        try:
            return context.levelStack[namespace]
        except AttributeError:
            context.levelStack = dict()
            addIndex = True
        except IndexError:
            addIndex = True

        if addIndex:
            levelStack = cls()
            context.levelStack[namespace] = levelStack
            f = lambda ineqs, context: levelStack.addToCurrentLevel(ineqs)
            context.addIneqListener.append(f)
            return context.levelStack[namespace]


    def __init__(self):
        self.currentLevel = 0
        self.levels = list()

    def setLevel(self, level):
        self.currentLevel = level
        while len(self.levels) <= level:
            self.levels.append(list())

    def addToCurrentLevel(self, ineqs):
        self.levels[self.currentLevel].extend(ineqs)

    def wipeLevel(self, level):
        if level >= len(self.levels):
            raise ValueError("Tried to wipe level %i that was never set."%(level))

        result = list()
        for i in range(level, len(self.levels)):
            result.extend(self.levels[i])
            self.levels[i].clear()

        return result

@register_rule
class SetLevel(EmptyRule):
    Id = "#"

    @classmethod
    def parse(cls, line, context):
        levelStack = LevelStack.setup(context)
        with WordParser(line) as words:
            level = words.nextInt()
            words.expectEnd()
        levelStack.setLevel(level)

        return cls()


@register_rule
class WipeLevel(EmptyRule):
    Id = "w"

    @classmethod
    def parse(cls, line, context):
        levelStack = LevelStack.setup(context)
        with WordParser(line) as words:
            level = words.nextInt()
            words.expectEnd()

        return cls(levelStack.wipeLevel(level))

    def __init__(self, deleteConstraints):
        self._deleteConstraints = deleteConstraints

    def deleteConstraints(self):
        return self._deleteConstraints
