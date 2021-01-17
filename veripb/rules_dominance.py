from veripb import InvalidProof
from veripb.rules import Rule, EmptyRule, register_rule
from veripb.rules import LevelStack
from veripb.rules import ReversePolishNotation, IsContradiction
from veripb.rules_register import register_rule
from veripb.parser import OPBParser, WordParser

from collections import deque


def rulesToDict(rules, default = None):
    res = {rule.Id: rule for rule in rules}
    if "" not in res:
        res[""] = default
    return res

def sortSubstitution(mapping):
    mapping.sort(key = lambda x: abs(x[0]))
    return zip(*mapping)

class SubContextInfo():
    def __init__(self):
        self.toDelete = []
        self.toAdd = []
        self.previousRules = None
        self.callbacks = []
        self.subgoals = deque()

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
        self.context = context

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
            callback(self.context, oldContext)

        if len(oldContext.subgoals) > 0:
            ineqId, ineq = oldContext.subgoals[0]

            ineq = self.context.ineqFactory.toString(ineq)
            raise InvalidProof("Open subgoal not proven: %i:, %s"%(ineqId, ineq))

        return oldContext

    def getCurrent(self):
        return self.infos[-1]

class TransitivityInfo:
    def __init__(self):
        self.fresh_right = []
        self.fresh_aux_1 = []
        self.fresh_aux_2 = []
        self.isProoven = False


class Order:
    def __init__(self, name = ""):
        self.name = name
        self.definition = []
        self.auxDefinition = None
        self.leftVars = []
        self.rightVars = []
        self.auxVars = []

        self.transitivity = TransitivityInfo()

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

class SubProof(EmptyRule):
    Id = "sub_proof"
    subRules = [EndOfProof, ReversePolishNotation, IsContradiction]

    # todo enforce only one def

    @classmethod
    def parse(cls, line, context):
        subcontexts = SubContext.setup(context)

        parentCtx = subcontexts.getCurrent()
        ineqId, nxtGoal = parentCtx.subgoals.popleft()
        subcontext = subcontexts.push()

        with WordParser(line) as words:
            goalId = words.nextInt()

        if goalId != ineqId:
            # todo try to be nice and figure stuff out by unit propagation
            raise InvalidProof("Missing proof for subgoal.")

        return cls(subcontext, nxtGoal)



    def __init__(self, subContext, constraint):
        self.subContext = subContext

        self.constraint = constraint.copy().negated()

        f = lambda context, subContext: self.check(context)
        subContext.callbacks.append(f)



    def compute(self, antecedents, context = None):
        return [self.constraint]

    def numConstraints(self):
        return 1

    def allowedRules(self, context, currentRules):
        self.subContext.previousRules = currentRules
        return rulesToDict(self.subRules)

    def check(self, context):
        if not getattr(context, "containsContradiction", False):
            raise InvalidProof("Irreflexivity proof did not show contradiction.")

        context.containsContradiction = False

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

class OrderDefinition(EmptyRule):
    Id = ""

    @classmethod
    def parse(cls, line, context):
        lits = []
        with WordParser(line) as words:
            parser = OPBParser(
                    ineqFactory = context.ineqFactory,
                    allowEq = True)
            ineqs = parser.parseConstraint(words)

        orderContext = OrderContext.setup(context)
        order = orderContext.activeDefinition
        order.definition.extend(ineqs)

        return cls()

class OrderDefinitions(EmptyRule):
    Id = "def"
    subRules = [EndOfProof, OrderDefinition]

    # todo enforce only one def

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

class Irreflexivity(EmptyRule):
    Id = "irreflexive"
    subRules = [EndOfProof, ReversePolishNotation, IsContradiction]

    @classmethod
    def parse(cls, line, context):
        subcontexts = SubContext.setup(context)
        subcontext = subcontexts.push()

        orderContext = OrderContext.setup(context)
        order = orderContext.activeDefinition

        return cls(subcontext, order)

    def __init__(self, subContext, order):
        self.subContext = subContext

        self.constraints = []
        for ineq in order.definition:
            ineq = ineq.copy()
            self.constraints.append(ineq)

        mapping = []
        mapping.extend(zip(order.leftVars,order.rightVars))
        mapping.extend(zip(order.rightVars,order.leftVars))
        frm, to = sortSubstitution(mapping)

        for ineq in order.definition:
            ineq = ineq.copy()

            ineq.substitute([],frm,to)
            self.constraints.append(ineq)

        f = lambda context, subContext: self.check(context)
        subContext.callbacks.append(f)

    def compute(self, antecedents, context = None):
        return self.constraints

    def numConstraints(self):
        return len(self.constraints)

    def check(self, context):
        if not getattr(context, "containsContradiction", False):
            raise InvalidProof("Irreflexivity proof did not show contradiction.")

        context.containsContradiction = False


    def allowedRules(self, context, currentRules):
        self.subContext.previousRules = currentRules
        return rulesToDict(self.subRules)


class TransitivityFreshRight(OrderVarsBase):
    Id = "fresh_right"

    @classmethod
    def addLits(cls, order, lits):
        order.transitivity.fresh_right.extend(lits)


class TransitivityFreshAux1(OrderVarsBase):
    Id = "fresh_aux1"

    @classmethod
    def addLits(cls, order, lits):
        order.transitivity.fresh_aux_1.extend(lits)

class TransitivityFreshAux2(OrderVarsBase):
    Id = "fresh_aux2"

    @classmethod
    def addLits(cls, order, lits):
        order.transitivity.fresh_aux_2.extend(lits)


class TransitivityVars(EmptyRule):
    # todo: add check that number of variables matches

    Id = "vars"
    subRules = [TransitivityFreshRight,TransitivityFreshAux1,TransitivityFreshAux2, EndOfProof]

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

class TransitivityProof(EmptyRule):
    Id = "proof"
    subRules = [EndOfProof, ReversePolishNotation, SubProof]

    @classmethod
    def parse(cls, line, context):
        subcontexts = SubContext.setup(context)
        subContext = subcontexts.push()

        orderContext = OrderContext.setup(context)
        order = orderContext.activeDefinition

        # This rule will fail if the proof is not correct so lets
        # already mark it proven here.
        order.transitivity.isProoven = True

        displayGoals = context.verifierSettings.trace

        return cls(context, subContext, order, context.firstFreeId, displayGoals)

    def __init__(self, context, subContext, order, freeId, displayGoals):
        self.subContext = subContext


        self.constraints = []
        freeId += len(order.definition)
        for ineq in order.definition:
            ineq = ineq.copy()
            self.constraints.append(ineq)

        substitution = []
        substitution.extend(zip(order.leftVars,order.rightVars))
        substitution.extend(zip(order.rightVars,order.transitivity.fresh_right))
        frm, to = sortSubstitution(substitution)

        freeId += len(order.definition)
        for ineq in order.definition:
            ineq = ineq.copy()

            ineq.substitute([],frm,to)
            self.constraints.append(ineq)


        substitution = []
        substitution.extend(zip(order.rightVars,order.transitivity.fresh_right))
        frm, to = sortSubstitution(substitution)

        for ineq in order.definition:
            ineq = ineq.copy()

            ineq.substitute([],frm,to)

            if displayGoals:
                ineqStr = context.ineqFactory.toString(ineq)
                print("  subgoal %00i: %s"%(freeId, ineqStr))

            self.subContext.subgoals.append((freeId, ineq))
            self.constraints.append(None)

            freeId += 1





    def compute(self, antecedents, context = None):
        return self.constraints

    def numConstraints(self):
        return len(self.constraints)

    def allowedRules(self, context, currentRules):
        self.subContext.previousRules = currentRules
        return rulesToDict(self.subRules)


class Transitivity(EmptyRule):
    Id = "transitivity"
    subRules = [EndOfProof,TransitivityVars,TransitivityProof]

    @classmethod
    def parse(cls, line, context):
        subcontexts = SubContext.setup(context)
        subcontext = subcontexts.push()

        orderContext = OrderContext.setup(context)
        order = orderContext.activeDefinition

        return cls(subcontext, order)

    def __init__(self, subContext, order):
        self.subContext = subContext
        self.order = order

        f = lambda context, subContext: self.check(context)
        subContext.callbacks.append(f)


    def check(self, context):
        if not self.order.transitivity.isProoven:
            raise InvalidProof("Transitivity proof is missing.")


    def allowedRules(self, context, currentRules):
        self.subContext.previousRules = currentRules
        return rulesToDict(self.subRules)


@register_rule
class StrictOrder(EmptyRule):
    Id = "strict_order"
    subRules = [OrderVars, OrderDefinitions, Irreflexivity, Transitivity, EndOfProof]

    @classmethod
    def parse(cls, line, context):
        name = line.strip()
        orders = OrderContext.setup(context)
        orders.newOrder(name)

        subcontexts = SubContext.setup(context)
        subcontext = subcontexts.push()

        f = lambda context, subContext: orders.qedNewOrder()
        subcontext.callbacks.append(f)

        return cls(subcontext)

    def __init__(self, subContext):
        self.subContext = subContext

    def allowedRules(self, context, currentRules):
        self.subContext.previousRules = currentRules
        return rulesToDict(self.subRules)