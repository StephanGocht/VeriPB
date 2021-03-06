import logging
import veripb.constraints
import mmap
import re
import itertools

from veripb.constraints import Term
from collections import defaultdict

from functools import partial
from veripb.exceptions import ParseError

from veripb.timed_function import TimedFunction


class RuleParserBase():
    commentChar = None

    def __init__(self, context):
        self.context = context
        try:
            self.context.addIneqListener
        except AttributeError:
            self.context.addIneqListener = list()

    def numRules(self, file):
        pos = file.tell()
        file.seek(0)

        num = 0
        for line in file:
            if not self.isEmpty(line):
                num += 1

        file.seek(pos)
        # don't count the proof header line
        return num - 1


    def isEmpty(self, line):
        if len(line) == 0 \
            or line == "\n" \
            or line[0] == self.commentChar \
            or ((line[0] == " " or line[0] == "\t")
                and line.strip() == ""):

            return True
        else:
            return False

    @TimedFunction.timeIter("RuleParserBase.parse")
    def parse(self, rules, file, dumpLine = False, defaultRule = None):
        self.rules = {rule.Id: rule for rule in rules}

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
            except StopIteration:
                raise ParseError("Expected Header.")
        else:
            lineNum = 0

        for line in lines:
            idSize = defaultIdSize
            lineNum += 1
            if dumpLine:
                print("line %03d: %s"% (lineNum, line.rstrip()))

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
                    yield step
                    for listener in self.context.addIneqListener:
                        listener(range(ineqId, ineqId + numConstraints), self.context)
                    ineqId += numConstraints

                except veripb.ParseError as e:
                    e.line = lineNum
                    if e.column is not None:
                        e.column += idSize
                    raise e

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
            version = words.expectNext("Expected version number.")
            major, minor = map(int, version.split("."))
            if major != 1 or minor < 0 or 1 < minor:
                raise ValueError("Unsupported version.")

            words.expectEnd()

def flatten(constraintList):
    result = list()
    for oneOrTwo in constraintList:
        for c in oneOrTwo:
            result.append(c)
    return result


class TermsUntil():
    def __init__(self, endWords, iterator, ineqFactory):
        self.iterator = iterator
        self.endWords = endWords
        self.lastWord = None
        self.ineqFactory = ineqFactory

    def parseLit(self, s):
        if s[0] == "~":
            return -self.ineqFactory.name2Num(s[1:])
        else:
            return self.ineqFactory.name2Num(s)

    def __next__(self):
        try:
            a = next(self.iterator)
        except StopIteration:
            raise ParseError("Expected %s."%\
                (",".join(map(lambda x: "'%s'"%(str(x)), self.endWords))))

        if a in self.endWords:
            self.lastWord = a
            raise StopIteration

        a = int(a)

        try:
            b = next(self.iterator)
        except StopIteration:
            raise ParseError("Expected literal.")

        b = self.parseLit(b)

        return (a,b)


    def __iter__(self):
        return self

class OPBParser():
    def __init__(self, ineqFactory, allowEq = True):
        self.ineqFactory = ineqFactory
        self.allowEq = allowEq

    @TimedFunction.time("OPBParser.parse")
    def parse(self, formulaFile):
        lines = iter(enumerate(formulaFile, start = 1))
        self.objective = None

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


        if self.objective is False:
            self.objective = None

        return {
                "numVariables": numVar,
                "numConstraints": numC,
                "constraints": constraints,
                "objective": self.objective
            }

    def parseHeader(self, line):
        with WordParser(line) as words:
            words.expectExact("*")
            words.expectExact("#variable=")
            numVar = words.nextInt()

            words.expectExact("#constraint=")
            numC = words.nextInt()

            return (numVar, numC)

    def parseLine(self, line):
        if len(line.strip()) == 0 or line[0] == "*":
            return []
        else:
            with WordParser(line) as words:
                if self.objective is None:
                    try:
                        peek = next(words)
                    except StopIteration:
                        return []
                    if peek == "min:":
                        self.parseObjective(words)
                        return []
                    else:
                        words.putback(peek)
                        self.objective = False

                result = self.parseOPB(words)
                words.expectEnd()
                return result

    def parseObjective(self, words):
        self.objective = dict()

        it = TermsUntil([";"], words, self.ineqFactory)
        for coeff, lit in it:
                if lit < 0:
                    lit = -lit
                    coeff = -coeff

                if lit in self.objective:
                    raise ValueError("Literal occuring twice in objective.")
                else:
                    self.objective[lit] = coeff


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
        it = iter(words)
        termIt = TermsUntil([">=","="], it, self.ineqFactory)
        terms = list(termIt)
        op = termIt.lastWord
        if op == "=" and not self.allowEq:
            return ValueError("Equality not allowed, only >= is allowed here.")

        try:
            degree = next(it)
        except StopIteration:
            raise ValueError("Expected degree.")
        if degree[-1] == ";":
            degree = int(degree[:-1])
        else:
            degree = int(degree)
            try:
                if (next(it) != ";"):
                    raise ValueError("Expecting ; at the end of the constraint.")
            except StopIteration:
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

        lineNum = 0
        for lineNum, line in lines:
            if not self.isEmpty(line):
                try:
                    numVar, numC = self.parseHeader(line)
                except ParseError as e:
                    e.line = lineNum
                    raise e
                break

        if numVar is None:
            raise ParseError("Expected header." ,line = lineNum + 1)

        constraints = list()
        for lineNum, line in lines:
            try:
                constraints.extend(self.parseLine(line.rstrip()))
            except ParseError as e:
                e.line = lineNum
                raise e

        if self.ineqFactory.numVars() != numVar:
            logging.warning("Number of variables did not match,"\
                " using %i instead."%(self.ineqFactory.numVars()))

        return {
                "numVariables": numVar,
                "numConstraints": numC,
                "constraints": constraints,
                "objective": None
            }

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

    def putback(self, word):
        self.wordIter = itertools.chain([word], self.wordIter)

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

    def expectNext(self, what = None):
        try:
            return next(self.wordIter)
        except StopIteration:
            if what is None:
                self.expectedWord()
            else:
                raise ValueError(what)

    def nextInt(self):
        return int(self.expectNext("Expected integer, got nothing."))

    def expectExact(self, what):
        nxt = self.expectNext("Expected '%s'."%(what))
        if (nxt != what):
            raise ValueError("Expected key word %s, got %s."%(what, nxt))

    def expectZero(self):
        self.expectExact("0")

    def expectEnd(self):
        try:
            next(self)
        except StopIteration:
            pass
        else:
            raise ValueError("Expected end of line!")
