import subprocess
import io
import re
from refpy.constraints import Inequality, Term

class SolverNotAvailable(RuntimeError):
    pass

class SolverIncompatible(SolverNotAvailable):
    pass


class Formula:
    def __init__(self, constraints):
        self.constraints = list(constraints)

        numVars = 0
        for c in self.constraints:
            for term in c.terms:
                coeff, lit = term
                var = abs(lit)
                numVars = max(numVars, var)

        self.numVars = numVars
        self.numConstraints = len(constraints)

    def write(self, stream):
        stream.write("* #variable= %i #constraint= %i\n"%(self.numVars, self.numConstraints))
        for c in self.constraints:
            try:
                stream.write(c.toOPB())
            except AttributeError:
                Inequality([Term(coeff, var) for coeff, var in c.terms], c.degree).toOPB()
            else:
                stream.write(";\n")

    def __str__(self):
        stream = io.StringIO()
        self.write(stream)
        return stream.getvalue()

class ParseRoundingSatResult():
    @classmethod
    def parse(cls, stream):
        try:
            stream = stream.split("\n")
        except AttributeError as e:
            pass

        parser = cls(stream)
        return parser.stat

    def __init__(self, stream):
        self.stat = dict()
        self._setupMatching()
        self._parse(stream)

    def _setupMatching(self):
        actions = list()
        actions.append((
            "s (UNSATISFIABLE|SATISFIABLE).*",
            self.setSAT))
        actions.append((
            r"c ([\w ]*\w)\s*:\s*(.*)",
            self.setOther))
        actions.append((
            r"d (\w+)\s+(.+)",
            self.setOther))
        actions.append((
            r".*OutOfMemory.*",
            self.setOOM))

        self.actions = [(re.compile(r), f) for r,f in actions]


    def _parse(self, stream):
        for line in stream:
            for regex, action in self.actions:
                match = regex.match(line)
                if match:
                    action(match)

    def setSAT(self, match):
        self.stat["result"] = match.group(1)

    def setOther(self, match):
        key = match[1].strip().replace(' ', '_')
        self.stat[key] = match[2]

    def setOOM(self, match):
        self.stat["result"] = "OutOfMemory"

class RoundingSat:
    executable="roundingsat"

    def __init__(self, executable = None):
        if executable is not None:
            self.executable = executable

    def solve(self, formula):
        r = subprocess.run(
            [self.executable],
            input=str(formula),
            stdout=subprocess.PIPE,
            encoding="utf-8")

        return ParseRoundingSatResult.parse(r.stdout)

    def propagatesToConflict(self, formula):
        result = self.solve(formula)
        try:
            return result["result"] == "UNSATISFIABLE" and int(result["conflicts"]) <= 1
        except KeyError:
            raise SolverIncompatible("Could not parse output, result or conflicts not found.")

def main():
    solver = RoundingSat()
    print(solver.propagatesToConflict(f))

if __name__ == '__main__':
    main()