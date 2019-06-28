import parsy

class ParseError(Exception):
    def __init__(self, error, fileName = None, line = None):
        self.error = error
        self.line  = line
        self.fileName  = fileName

    def __str__(self):
        if isinstance(self.error, parsy.ParseError):
            return str(ParsyErrorAdapter(self.error, self.fileName, self.line))
        else:
            return "%s:%s: %s"%(self.fileName, self.line, str(self.error))

class ParsyErrorAdapter(parsy.ParseError):
    def __init__(self, orig, fileName, line = None):
        super().__init__(orig.expected, orig.stream, orig.index)
        self.fileName = fileName
        self.line = line

    def line_info(self):
        lineInfo = super().line_info()
        lineInfoSplit = lineInfo.split(":")
        if len(lineInfoSplit) == 2:
            line = int(lineInfoSplit[0]) + 1
            col  = int(lineInfoSplit[1]) + 1
            if self.line is not None:
                line = self.line
            lineInfo = "%i:%i"%(line, col)
        elif self.line is not None:
            lineInfo = "%i:%i"%(self.line, self.index + 1)
        return "%s:%s"%(self.fileName, lineInfo)


class InvalidProof(Exception):
    pass
