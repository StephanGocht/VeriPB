registered_rules = []
def register_rule(rule):
    registered_rules.append(rule)
    return rule

def get_registered_rules():
    import veripb.rules
    import veripb.rules_dominance
    import veripb.rules_debug

    return registered_rules

def get_dom_friendly_rules():
    import veripb.rules
    import veripb.rules_dominance

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
        veripb.rules_dominance.AddRedundant
    ]

dom_rules = None

def dom_friendly_rules():
    global dom_rules
    if dom_rules is None:
        dom_rules = get_dom_friendly_rules()
    return dom_rules


def rules_to_dict(rules, default = None):
    res = dict()
    for rule in rules:
        for Id in rule.Ids:
            res[Id] = rule
    if "" not in res:
        res[""] = default
    return res