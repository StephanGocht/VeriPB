import parsy
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
            return parsy.fail("Unsuported rule.")

        return parsy.success(Id)

    def parseHeader(self, line, lineNum):
        headerParser = parsy.regex(r"refutation using").\
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

        try:
            headerParser.parse(line)
        except parsy.ParseError as e:
            raise ParseError(e, line = lineNum)

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

                except parsy.ParseError as e:
                    raise ParseError(e, line = lineNum)
                except ValueError as e:
                    raise ParseError(e, line = lineNum)
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


        self.slowParser = self.getOPBParser(allowEq)

    def parse(self, formulaFile):
        numVar = (parsy.regex(r" *#variable *= *") >> parsy.regex(r"[0-9]+")) \
                        .map(int) \
                        .desc("Number of variables in the form '#variable = [0-9]+'")
        numC = (parsy.regex(r" *#constraint *= *") >> parsy.regex(r"[0-9]+")) \
                        .map(int) \
                        .desc("Number of constraints in the form '#constraint = [0-9]+'")

        eol = parsy.regex(" *\n").desc("return at end of line").many()
        emptyLine = parsy.regex(r"(\s+)").desc("empty line")
        commentLine = parsy.regex(r"(\s*\*.*)").desc("comment line starting with '*'")
        header = (parsy.regex(r"\* ") >> parsy.seq(numVar, numC) << eol) \
                        .desc("header line in form of '* #variable = [0-9]+ #constraint = [0-9]+'")

        nothing = (emptyLine << eol | commentLine << eol).map(lambda x: [])

        lines = iter(enumerate(formulaFile, start = 1))

        lineNum, line = next(lines)
        try:
            numVar, numC = header.parse(line)
        except parsy.ParseError as e:
            raise ParseError(e, line = lineNum)

        constraints = list()
        for lineNum, line in lines:
            line = line.rstrip()
            try:
                constraints.extend(self.parseLine(line))
            except parsy.ParseError as e:
                raise ParseError(e, line = lineNum)
            except ValueError as e:
                raise ParseError(e, line = lineNum)

        return ((numVar, numC), constraints)

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
        lits = list()
        try:
            nxt = words.nextInt()
            while (nxt != 0):
                lits.append(nxt)
                nxt = words.nextInt()
        except StopIteration:
            raise ValueError("Expected 0 at end of constraint.")

        return [self.ineqFactory.fromTerms([Term(1,l) for l in lits], 1)]

    def parseOPB(self, wordParser):
        def parseVar(s):
            if s[0] == "~":
                return -self.ineqFactory.name2Num(s[1:])
            else:
                return self.ineqFactory.name2Num(s)

        terms = list()
        it = wordParser
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

    def getOPBConstraintParser(self, allowEq = True):
        def lit2int(sign, num):
            if sign == "~":
                return -self.ineqFactory.name2Num(num)
            else:
                return self.ineqFactory.name2Num(num)

        space    = parsy.regex(r" +").desc("space")
        coeff    = parsy.regex(r"[+-]?[0-9]+").map(int).desc("integer for the coefficient (make sure to not have spaces between the sign and the degree value)")
        variable = parsy.seq(parsy.regex(r"~").optional(), parsy.regex(r"[a-zA-Z][\w\^\{\}\[\]-]*")).combine(lit2int).desc("variable in the form '~?x[1-9][0-9]*'")
        term     = parsy.seq(coeff << space, variable << space).map(tuple)

        if allowEq:
            equality = parsy.regex(r"(=|>=)").desc("= or >=")
        else:
            equality = parsy.regex(r">=").desc(">=")

        degree   = space >> parsy.regex(r"[+-]?[0-9]+").map(int).desc("integer for the degree")

        end      = parsy.regex(r";").desc("termination of rhs with ';'")
        finish   = space.optional() >> end

        return parsy.seq(space.optional() >> term.many(), equality, degree << finish).map(tuple)

    def getCNFConstraintParser(self):
        def f(lit):
            var = self.ineqFactory.name2Num("x%i"%abs(lit))
            if lit < 0:
                return -var
            else:
                return var

        space    = parsy.regex(r" +").desc("space")
        literal  = parsy.regex(r"[+-]?[1-9][0-9]*").desc("literal").map(int)
        eol      = parsy.regex(r"0").desc("end of line zero")

        return (literal << space).many() << eol

    def fromParsy(self, t):
        result = list()

        if isinstance(t, list):
            # we got a clause from getCNFParser
            result.append(self.ineqFactory.fromTerms([Term(1,l) for l in t], 1))
        else:
            # we got a tuple containing a constraint
            terms, eq, degree = t

            result.append(self.ineqFactory.fromTerms([Term(a,x) for a,x in terms], degree))
            if eq == "=":
                result.append(self.ineqFactory.fromTerms([Term(-a,x) for a,x in terms], -degree))

        return parsy.success(result)

    def getOPBParser(self, allowEq = True):
        def f(t):
            return self.fromParsy(t)

        return self.getOPBConstraintParser(allowEq).bind(f)

    def getCNFParser(self):
        def f(t):
            return self.fromParsy(t)

        return self.getCNFConstraintParser().bind(f)

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