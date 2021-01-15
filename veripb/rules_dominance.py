from veripb import InvalidProof
from veripb.rules import Rule, EmptyRule, register_rule
from veripb.rules import LevelStack
from veripb.rules_register import register_rule
from veripb.parser import OPBParser, WordParser


def rulesToDict(rules, default = None):
    res = {rule.Id: rule for rule in rules}
    res[""] = default
    return res

class SubContextInfo():
    def __init__(self):
        self.toDelete = []
        self.toAdd = []
        self.previousRules = None
        self.callbacks = []

    def addToDelete(self, ineqs):
        self.toDelete.extend(ineqs)

class SubContext():
    @classmethod
    def setup(cls, context):
        try:
            return context.subContexts
        except AttributeError:
            context.subContexts = cls(context)
            return context.subContexts

    def __init__(self, context):
        self.infos = []

        f = lambda ineqs, context: self.addToDelete(ineqs)
        context.addIneqListener.append(f)

    def addToDelete(self, ineqs):
        if len(self.infos) > 0:
            self.infos[-1].addToDelete(ineqs)

    def push(self):
        newSubContext = SubContextInfo()
        self.infos.append(newSubContext)
        return self.infos[-1]

    def pop(self):
        oldContext = self.infos.pop()
        for callback in oldContext.callbacks:
            callback(oldContext)
        return oldContext

    def getCurrent(self):
        return self.infos[-1]

class Order:
    def __init__(self, name = ""):
        self.name = name
        self.definition = None
        self.auxDefinition = None
        self.leftVars = []
        self.rightVars = []
        self.auxVars = []

    def check(self):
        # todo: check that all proof goals were met
        # raise InvalidProof("Failed to add order.")
        pass

class OrderContext:
    @classmethod
    def setup(cls, context):
        try:
            return context.orderContext
        except AttributeError:
            context.orderContext = cls()
            return context.orderContext

    def __init__(self):
        self.orders = dict()

        emptyOrder = Order()
        self.activeOrder = emptyOrder
        self.orders[emptyOrder.name] = emptyOrder

        self.activeDefinition = None

    def newOrder(self, name):
        newOrder = Order(name)
        if self.activeDefinition is not None:
            raise ValueError("Tried to create new order while previous"
                "order definition was not finished.")

        self.activeDefinition = newOrder

    def qedNewOrder(self):
        newOrder = self.activeDefinition
        newOrder.check()
        self.orders[newOrder.name] = newOrder
        self.activeDefinition = None

        return []

class EndOfProof(EmptyRule):
    Id = "qed"

    @classmethod
    def parse(cls, line, context):
        subcontexts = SubContext.setup(context)
        subcontext = subcontexts.pop()
        return cls(subcontext)

    def __init__(self, subcontext):
        self.subcontext = subcontext

    def deleteConstraints(self):
        return self.subcontext.toDelete

    def compute(self, antecedents, context = None):
        return self.subcontext.toAdd

    def allowedRules(self, context, currentRules):
        if self.subcontext.previousRules is not None:
            return self.subcontext.previousRules
        else:
            return currentRules

class OrderVarsBase(EmptyRule):
    @classmethod
    def addLits(cls, order, lits):
        pass

    @classmethod
    def parse(cls, line, context):
        lits = []
        with WordParser(line) as words:
            for nxt in words:
                lits.append(context.ineqFactory.lit2int(nxt))

        order = OrderContext.setup(context)
        cls.addLits(order.activeDefinition, lits)
        return cls()


class LeftVars(OrderVarsBase):
    Id = "left"

    @classmethod
    def addLits(cls, order, lits):
        order.leftVars.extend(lits)

class RightVars(OrderVarsBase):
    Id = "right"

    @classmethod
    def addLits(cls, order, lits):
        order.rightVars.extend(lits)

class AuxVars(OrderVarsBase):
    Id = "aux"

    @classmethod
    def addLits(cls, order, lits):
        order.auxVars.extend(lits)

class OrderVars(EmptyRule):
    # todo: add check that number of variables matches

    Id = "vars"
    subRules = [LeftVars, RightVars, AuxVars, EndOfProof]

    @classmethod
    def parse(cls, line, context):
        subcontexts = SubContext.setup(context)
        subcontext = subcontexts.push()

        return cls(subcontext)

    def __init__(self, subContext):
        self.subContext = subContext

    def allowedRules(self, context, currentRules):
        self.subContext.previousRules = currentRules
        return rulesToDict(self.subRules)

@register_rule
class StrictOrder(EmptyRule):
    Id = "strict_order"
    subRules = [OrderVars, EndOfProof]

    @classmethod
    def parse(cls, line, context):
        name = line.strip()
        orders = OrderContext.setup(context)
        orders.newOrder(name)

        subcontexts = SubContext.setup(context)
        subcontext = subcontexts.push()

        f = lambda subContext: orders.qedNewOrder()
        subcontext.callbacks.append(f)

        return cls(subcontext)

    def __init__(self, subContext):
        self.subContext = subContext

    def allowedRules(self, context, currentRules):
        self.subContext.previousRules = currentRules
        return rulesToDict(self.subRules)