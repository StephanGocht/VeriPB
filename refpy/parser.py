import logging
import refpy.constraints
import mmap
import re

from refpy.constraints import Term

from functools import partial
from refpy.exceptions import ParseError

class RuleParser():
    def __init__(self, context):
        self.context = context
        try:
            self.context.addIneqListener
        except AttributeError:
            self.context.addIneqListener = list()

    def checkIdentifier(self, Id):
        if not Id in self.rules:
            raise ValueError("Unsuported rule '%s'."%(Id))

    def parseHeader(self, line, lineNum):
        with WordParser(line) as words:
            first = next(words)
            if first != "refutation" and first != "proof":
                raise ValueError("Expected header starting with 'proof'")
            words.expectExact("using")

            try:
                nxt = next(words)
                while (nxt != "0"):
                    self.checkIdentifier(nxt)
                    nxt = next(words)
            except StopIteration:
                raise ValueError("Expected 0 at end of constraint.")

            words.expectEnd()

    def isEmpty(self, line):
        if  len(line) == 0 \
            or line == "\n" \
            or line[0] == "*" \
            or ((line[0] == " " or line[0] == "\t")
                and line.strip() == ""):

            return True
        else:
            return False

    def parse(self, rules, file):
        self.rules = {rule.Id: rule for rule in rules}
        result = list()

        lineNum = 1
        ineqId = 1
        lines = iter(file)

        idSize = 1
        # the first line is not allowed to be comment line or empty but must be the header
        self.parseHeader(next(lines), lineNum)

        for line in lines:
            lineNum += 1

            if not self.isEmpty(line):
                try:
                    rule = self.rules[line[0:idSize]]
                except KeyError as e:
                    raise ParseError("Unsupported rule '%s'"%(line[0]), line = lineNum)

                try:
                    step = rule.parse(line[idSize:], self.context)
                    numConstraints = step.numConstraints()
                    result.append(step)
                    for listener in self.context.addIneqListener:
                        listener(range(ineqId, ineqId + numConstraints), self.context)
                    ineqId += numConstraints

                except refpy.ParseError as e:
                    e.line = lineNum
                    e.column += idSize
                    raise e

        return result

def flatten(constraintList):
    result = list()
    for oneOrTwo in constraintList:
        for c in oneOrTwo:
            result.append(c)
    return result

class OPBParser():
    def __init__(self, ineqFactory, allowEq = True):
        self.ineqFactory = ineqFactory
        self.allowEq = allowEq

    def parse(self, formulaFile):
        lines = iter(enumerate(formulaFile, start = 1))

        try:
            lineNum, line = next(lines)
        except StopIteration:
            raise ParseError("Expected header." ,line = 1)

        try:
            numVar, numC = self.parseHeader(line)
        except ParseError as e:
            e.line = lineNum
            raise e

        constraints = list()
        for lineNum, line in lines:
            try:
                constraints.extend(self.parseLine(line.rstrip()))
            except ParseError as e:
                e.line = lineNum
                raise e

        return ((numVar, numC), constraints)

    def parseHeader(self, line):
        with WordParser(line) as words:
            words.expectExact("*")
            words.expectExact("#variable=")
            numVar = words.nextInt()

            words.expectExact("#constraint=")
            numC = words.nextInt()

            return (numVar, numC)

    def parseLine(self, line):
        if len(line) == 0 or line[0] == "*":
            return []
        else:
            with WordParser(line) as words:
                result = self.parseOPB(words)
                words.expectEnd()
                return result

    def parseConstraint(self, words):
        constraintType = next(words)
        if constraintType == "opb":
            ineq = self.parseOPB(words)
        elif constraintType == "cnf":
            ineq = self.parseCNF(words)
        else:
            raise ValueError("Unnown constraint type.")
        return ineq

    def parseCNF(self, words):
        words = iter(words)
        lits = list()
        try:
            nxt = int(next(words))
            while (nxt != 0):
                lits.append(nxt)
                nxt = int(next(words))
        except StopIteration:
            raise ValueError("Expected 0 at end of constraint.")

        return [self.ineqFactory.fromTerms([Term(1,l) for l in lits], 1)]

    def parseOPB(self, words):
        def parseVar(s):
            if s[0] == "~":
                return -self.ineqFactory.name2Num(s[1:])
            else:
                return self.ineqFactory.name2Num(s)
        it = iter(words)
        terms = list()
        nxt = next(it)
        while (nxt not in [">=","="]):
            terms.append((int(nxt), parseVar(next(it))))
            nxt = next(it)

        op = nxt
        if op == "=" and not self.allowEq:
            return ValueError("Equality not allowed, only >= is allowed here.")

        degree = next(it)
        if degree[-1] == ";":
            degree = int(degree[:-1])
        else:
            degree = int(degree)
            if (next(it) != ";"):
                raise ValueError("Expecting ; at the end of the constraint.")

        result = [self.ineqFactory.fromTerms(terms, degree)]
        if op == "=":
            result.append(self.ineqFactory.fromTerms([(-a,l) for a,l in terms], -degree))
        return result


class WordParser():
    def __init__(self, line):
        self.line = line
        self.words = line.split()
        self.pos = -1

    def __iter__(self):
        return self

    def __next__(self):
        self.pos += 1
        if (self.pos >= len(self.words)):
            raise StopIteration(self)
        return self.words[self.pos]

    def __enter__(self):
        return self

    def __exit__(self, exec_type, exec_value, exec_traceback):
        if exec_type is not None:
            if issubclass(exec_type, ValueError):
                self.raiseParseError(self.pos, exec_value)
            if issubclass(exec_type, StopIteration) \
                    and exec_value.value is self:
                self.expectedWord()

    def raiseParseError(self, word, error):
        column = None
        if self.pos >= len(self.words):
            column = len(self.line)
        else:
            for idx, match in enumerate(re.finditer(r" *(?P<word>[^ ]+)", self.line)):
                if idx == self.pos:
                    column = match.start("word")

        assert(column is not None)

        column += 1
        raise ParseError(error, column = column)

    def expectedWord(self):
        self.raiseParseError(self.pos, "Expected another word.")

    def next(self, what = None):
        try:
            return next(self)
        except StopIteration:
            if what is None:
                self.expectedWord()
            else:
                raise ValueError(what)

    def nextInt(self):
        return int(self.next("Expected integer, got nothing."))

    def expectExact(self, what):
        nxt = next(self)
        if (nxt != what):
            raise ValueError("Expected %s, got %s."%(what, nxt))

    def expectZero(self):
        if (next(self) != "0"):
            raise ValueError("Expected 0.")

    def expectEnd(self):
        try:
            next(self)
        except StopIteration:
            pass
        else:
            raise ValueError("Expected end of line!")