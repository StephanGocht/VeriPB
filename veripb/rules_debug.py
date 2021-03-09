from veripb import InvalidProof
from veripb.rules import Rule, EmptyRule
from veripb.rules_register import register_rule
from veripb.parser import OPBParser, WordParser

@register_rule
class IsDeleted(EmptyRule):
    Ids = ["is_deleted"]

    @classmethod
    def parse(cls, line, context):
        with WordParser(line) as words:
            parser = OPBParser(
                ineqFactory = context.ineqFactory,
                allowEq = False)
            ineq = parser.parseConstraint(words)
            found = context.propEngine.getDeletions(ineq[0])

        return cls(found)

    def __init__(self, found):
        self.found = found

    def compute(self, antecedents, context):
        if self.found:
            raise InvalidProof("Constraint should be deleted.")
        return []