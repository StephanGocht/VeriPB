class ParseError(Exception):
    def __init__(self, error, fileName = None, line = None, column = None):
        self.error = error
        self.line  = line
        self.column = column
        self.fileName  = fileName

    def __str__(self):
        s = ""
        for l in [self.fileName, self.line, self.column]:
            if l is not None:
                s += "%s:"%str(l)
            else:
                s += "?:"
        s += " " + str(self.error)
        return s

class DirectParseError(ParseError):
    def __str__(self):
        return str(self.error)

class InvalidProof(Exception):
    pass
