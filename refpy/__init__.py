import parsy

class ParseError(parsy.ParseError):
    def __init__(self, orig, fileName):
        super().__init__(orig.expected, orig.stream, orig.index)
        self.fileName = fileName

    def line_info(self):
        lineInfo = super().line_info()
        lineInfoSplit = lineInfo.split(":")
        if len(lineInfoSplit) == 2:
            lineInfo = "%i:%i"%(int(lineInfoSplit[0]) + 1, int(lineInfoSplit[1]) + 1)
        return "%s:%s"%(self.fileName, lineInfo)

class InvalidProof(Exception):
    pass

from refpy.utils import run,runUI,run_cmd_main