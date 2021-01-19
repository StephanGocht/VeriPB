registered_rules = []
def register_rule(rule):
    registered_rules.append(rule)
    return rule

def get_registered_rules():
    import veripb.rules
    import veripb.rules_dominance

    return registered_rules

def dom_friendly_rules():
    import veripb.rules

    return [
        veripb.rules.DeleteConstraints,
        veripb.rules.Assumption,
        veripb.rules.ReverseUnitPropagation,
        veripb.rules.ConstraintEquals,
        veripb.rules.ConstraintImplies,
        veripb.rules.ConstraintImpliesGetImplied,
        veripb.rules.IsContradiction,
        veripb.rules.ReversePolishNotation,
        veripb.rules.SetLevel,
        veripb.rules.WipeLevel,
    ]
