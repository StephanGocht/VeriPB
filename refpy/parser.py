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

    def parse(self, rules, file):
        self.rules = {rule.Id: rule for rule in rules}
        result = list()

        lineNum = 1
        lines = iter(file)
        self.parseHeader(next(lines), lineNum)

        for line in lines:
            lineNum += 1

            try:
                rule = self.rules[line[0]]
            except KeyError as e:
                raise ParseError("Unsupported rule %s"%(line[0]), line = lineNum)

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
    space    = parsy.regex(r" +").desc("space").optional()
    coeff    = parsy.regex(r"[+-]?[0-9]+").map(int).desc("integer for the coefficient (make sure to not have spaces between the sign and the degree value)")
    variable = (parsy.regex(r"x") >> parsy.regex(r"[1-9][0-9]*").map(int)).desc("variable in the form 'x[1-9][0-9]*'")
    term     = parsy.seq(space >> coeff, space >> variable).map(tuple)

    if allowEq:
        equality = space >> parsy.regex(r"(=|>=)").desc("= or >=")
    else:
        equality = space >> parsy.regex(r">=").desc(">=")

    degree   = space >> parsy.regex(r"[+-]?[0-9]+").map(int).desc("integer for the degree")

    end      = parsy.regex(r";").desc("termination of rhs with ';'")
    finish   = space >> end

    return parsy.seq(term.many(), equality, degree << finish).map(tuple)


def flatten(constraintList):
    result = list()
    for oneOrTwo in constraintList:
        for c in oneOrTwo:
            result.append(c)
    return result

def getOPBParser():
    numVar = (parsy.regex(r" *#variable *= *") >> parsy.regex(r"[0-9]+")) \
                    .map(int) \
                    .desc("Number of variables in the form '#variable = [0-9]+'")
    numC = (parsy.regex(r" *#constraint *= *") >> parsy.regex(r"[0-9]+")) \
                    .map(int) \
                    .desc("Number of constraints in the form '#constraint = [0-9]+'")

    eol = parsy.regex(" *\n").desc("return at end of line")
    emptyLine = parsy.regex(r"(\s*)").desc("empty line")
    commentLine = parsy.regex(r"(\s*\*.*)").desc("comment line starting with '*'")
    header = (parsy.regex(r"\* ") >> parsy.seq(numVar, numC) << eol) \
                    .desc("header line in form of '* #variable = [0-9]+ #constraint = [0-9]+'")

    nothing = (emptyLine << eol | commentLine << eol).many()

    constraint = getOPBConstraintParser().bind(refpy.constraints.Inequality.fromParsy)

    return parsy.seq(header, (nothing >> constraint << eol).many().map(flatten)) \
     + ((emptyLine | commentLine).map(lambda x: []) | constraint.map(lambda x: [x])) << eol.optional()