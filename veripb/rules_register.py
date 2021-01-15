registered_rules = []
def register_rule(rule):
    registered_rules.append(rule)
    return rule

def get_registered_rules():
    import veripb.rules
    import veripb.rules_dominance

    return registered_rules