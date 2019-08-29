import parsy
import logging
import refpy.constraints
import mmap

from functools import partial
from refpy.exceptions import ParseError

class RuleParser():
    def checkIdentifier(self, Id):
        if not Id in self.rules:
            return parsy.fail("Unsuported rule.")

        return parsy.success(Id)


    def _parse(self, rules, stream):
        logging.info("Available Rules: %s"%(", ".join(["%s"%(rule.Id) for rule in rules])))

        self.identifier = set([rule.Id for rule in rules])

        skipBlankLines = parsy.regex(r"\s*")
        singleRules = [skipBlankLines >> parsy.regex(rule.Id).desc("rule identifier") >> rule.getParser() << skipBlankLines for rule in rules]
        anyRuleParser = parsy.alt(*singleRules)

        parser = header >> anyRuleParser.many()

        return parser.parse(stream)

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
        lines = iter(file)
        # the first line is not allowed to be comment line or empty but must be the header
        self.parseHeader(next(lines), lineNum)

        for line in lines:
            lineNum += 1

            if not self.isEmpty(line):
                try:
                    rule = self.rules[line[0]]
                except KeyError as e:
                    raise ParseError("Unsupported rule '%s'"%(line[0]), line = lineNum)

                try:
                    result.append(rule.parse(line[1:]))
                except parsy.ParseError as e:
                    raise ParseError(e, line = lineNum)
                except ValueError as e:
                    raise ParseError(e, line = lineNum)

        return result



def getCNFConstraintParser():
    space    = parsy.regex(r" +").desc("space")
    literal  = parsy.regex(r"[+-]?[1-9][0-9]*").desc("literal").map(int)
    eol      = parsy.regex(r"0").desc("end of line zero")

    return (literal << space).many() << eol

def getOPBConstraintParser(allowEq = True):
    def lit2int(sign, num):
        if sign == "~":
            return -int(num)
        else:
            return int(num)

    space    = parsy.regex(r" +").desc("space")
    coeff    = parsy.regex(r"[+-]?[0-9]+").map(int).desc("integer for the coefficient (make sure to not have spaces between the sign and the degree value)")
    variable = parsy.seq(parsy.regex(r"~").optional(), parsy.regex(r"x") >> parsy.regex(r"[1-9][0-9]*")).combine(lit2int).desc("variable in the form '~?x[1-9][0-9]*'")
    term     = parsy.seq(coeff << space, variable << space).map(tuple)

    if allowEq:
        equality = parsy.regex(r"(=|>=)").desc("= or >=")
    else:
        equality = parsy.regex(r">=").desc(">=")

    degree   = space >> parsy.regex(r"[+-]?[0-9]+").map(int).desc("integer for the degree")

    end      = parsy.regex(r";").desc("termination of rhs with ';'")
    finish   = space.optional() >> end

    return parsy.seq(space.optional() >> term.many(), equality, degree << finish).map(tuple)


def flatten(constraintList):
    result = list()
    for oneOrTwo in constraintList:
        for c in oneOrTwo:
            result.append(c)
    return result

class OPBParser():
    def __init__(self, ineqFactory = None, allowEq = True):
        if ineqFactory is None:
            self.ineqFactory = refpy.constraints.defaultFactory

        self.allowEq = allowEq
        self.slowParser = self.ineqFactory.getOPBParser(allowEq)

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
        try:
            return self.parseLineQuick(line)
        except ValueError as valueError:
            try:
                self.slowParser.parse(line)
            except parsy.ParseError as e:
                raise e
            else:
                # if the parsy parser can parse it we do not want to
                # silenty work as this means that the parsy
                # specification is not precise enough
                raise valueError

    def parseLineQuick(self, line):
        def parseVar(s):
            if s[0] == "x":
                return int(s[1:])
            elif s[0:2] == "~x":
                return -int(s[2:])
            else:
                raise ValueError("expecting literal, got '%s'" % s)

        if len(line) == 0 or line[0] == "*":
            return []

        terms = list()
        it = iter(line.split())
        nxt = next(it)
        while (nxt not in [">=","="]):
            terms.append((int(nxt), parseVar(next(it))))
            nxt = next(it)

        op = nxt
        degree = next(it)
        if degree[-1] == ";":
            degree = degree[:-1]
        degree = int(degree)

        result = [self.ineqFactory.fromTerms(terms, degree)]
        if op == "=":
            if not self.allowEq:
                return ValueError()
            result.append(self.ineqFactory.fromTerms([(-a,l) for a,l in terms], -degree))
        return result