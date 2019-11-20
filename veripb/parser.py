import logging
import veripb.constraints
import mmap
import re

from veripb.constraints import Term

from functools import partial
from veripb.exceptions import ParseError

class RuleParserBase():
    commentChar = None

    def __init__(self, context):
        self.context = context
        try:
            self.context.addIneqListener
        except AttributeError:
            self.context.addIneqListener = list()

    def isEmpty(self, line):
        if len(line) == 0 \
            or line == "\n" \
            or line[0] == self.commentChar \
            or ((line[0] == " " or line[0] == "\t")
                and line.strip() == ""):

            return True
        else:
            return False

    def parse(self, rules, file, defaultRule = None):
        self.rules = {rule.Id: rule for rule in rules}
        result = list()

        lineNum = 1
        ineqId = 1
        lines = iter(file)

        defaultIdSize = 1
        # the first line is not allowed to be comment line or empty but must be the header
        if hasattr(self, "parseHeader"):
            try:
                self.parseHeader(next(lines))
            except ParseError as e:
                e.line = lineNum
                raise e

        for line in lines:
            idSize = defaultIdSize
            lineNum += 1

            if not self.isEmpty(line):
                try:
                    rule = self.rules[line[0:idSize]]
                except KeyError as e:
                    if defaultRule is None:
                        raise ParseError("Unsupported rule '%s'"%(line[0]), line = lineNum)
                    else:
                        rule = defaultRule
                        idSize = 0

                try:
                    step = rule.parse(line[idSize:], self.context)
                    step.lineInFile = lineNum
                    numConstraints = step.numConstraints()
                    result.append(step)
                    for listener in self.context.addIneqListener:
                        listener(range(ineqId, ineqId + numConstraints), self.context)
                    ineqId += numConstraints

                except veripb.ParseError as e:
                    e.line = lineNum
                    e.column += idSize
                    raise e

        return result

class RuleParser(RuleParserBase):
    commentChar = "*"

    def checkIdentifier(self, Id):
        if not Id in self.rules:
            raise ValueError("Unsuported rule '%s'."%(Id))

    def parseHeader(self, line):
        with WordParser(line) as words:
            words.expectExact("pseudo-Boolean")
            words.expectExact("proof")
            words.expectExact("version")
            version = next(words)
            major, minor = map(int, version.split("."))
            if major != 1 or minor < 0 or 0 < minor:
                raise ValueError("Unsupported version.")

            words.expectEnd()

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

        if self.ineqFactory.numVars() != numVar:
            logging.warn("Number of variables did not match,"\
                " using %i instead."%(self.ineqFactory.numVars()))

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
        ineq = self.parseOPB(words)
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

        return [self.ineqFactory.fromTerms([Term(1,self.ineqFactory.intlit2int(l)) for l in lits], 1)]

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

class CNFParser():
    def __init__(self, ineqFactory, allowEq = True):
        self.ineqFactory = ineqFactory
        self.allowEq = allowEq

    def parse(self, formulaFile):
        lines = iter(enumerate(formulaFile, start = 1))

        numVar, numC = None, None
        for lineNum, line in lines:
            if not self.isEmpty(line):
                try:
                    numVar, numC = self.parseHeader(line)
                except ParseError as e:
                    e.line = lineNum
                    raise e
                break

        if numVar is None:
            raise ParseError("Expected header." ,line = len(lines) + 1)

        constraints = list()
        for lineNum, line in lines:
            try:
                constraints.extend(self.parseLine(line.rstrip()))
            except ParseError as e:
                e.line = lineNum
                raise e

        if self.ineqFactory.numVars() != numVar:
            logging.warn("Number of variables did not match,"\
                " using %i instead."%(self.ineqFactory.numVars()))

        return ((numVar, numC), constraints)

    def parseHeader(self, line):
        with WordParser(line) as words:
            words.expectExact("p")
            words.expectExact("cnf")
            numVar = words.nextInt()
            numC = words.nextInt()

            return (numVar, numC)

    def isEmpty(self,line):
        return len(line) == 0 or line[0] == "c" or len(line.strip()) == 0

    def parseLine(self, line):
        if self.isEmpty(line):
            return []
        else:
            with WordParser(line) as words:
                result = self.parseCNF(words)
                words.expectEnd()
                return result

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

        return [self.ineqFactory.fromTerms([Term(1,self.ineqFactory.intlit2int(l)) for l in lits], 1)]

class WordParser():
    def __init__(self, line):
        self.line = line
        self.words = line.split()
        self.wordIter = iter(line.split())

    def __iter__(self):
        return self.wordIter

    def __next__(self):
        return next(self.wordIter)

    def __enter__(self):
        return self

    def __exit__(self, exec_type, exec_value, exec_traceback):
        if exec_type is not None:
            wordNum = 0
            for x in self.wordIter:
                wordNum += 1
            wordNum = len(self.words) - wordNum - 1

            if issubclass(exec_type, ValueError):
                self.raiseParseError(wordNum, exec_value)
            if issubclass(exec_type, StopIteration) \
                    and exec_value.value is self:
                self.expectedWord()

    def raiseParseError(self, wordNum, error):
        column = None
        if wordNum >= len(self.words):
            column = len(self.line)
        else:
            for idx, match in enumerate(re.finditer(r" *(?P<word>[^ ]+)", self.line)):
                if idx == wordNum:
                    column = match.start("word")

        assert(column is not None)

        column += 1
        raise ParseError(error, column = column)

    def expectedWord(self):
        self.raiseParseError(len(self.words), "Expected another word.")

    def next(self, what = None):
        try:
            return next(self.wordIter)
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
            raise ValueError("Expected key word %s, got %s."%(what, nxt))

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