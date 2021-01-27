from veripb import InvalidProof
from veripb.rules import Rule, EmptyRule, register_rule
from veripb.rules import ReversePolishNotation, IsContradiction
from veripb.rules_register import register_rule, dom_friendly_rules
from veripb.parser import OPBParser, WordParser

from collections import deque


def rulesToDict(rules, default = None):
    res = {rule.Id: rule for rule in rules}
    if "" not in res:
        res[""] = default
    return res

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

    def pop(self, checkSubgoals = True):
        oldContext = self.infos.pop()
        for callback in oldContext.callbacks:
            callback(self.context, oldContext)

        if checkSubgoals and len(oldContext.subgoals) > 0:
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
        self.isProven = False


class Order:
    def __init__(self, name = ""):
        self.name = name
        self.definition = []
        self.auxDefinition = None
        self.leftVars = []
        self.rightVars = []
        self.auxVars = []

        # variables the order is actually defined over, will be set
        # when order is loaded.
        self.vars = []

        # Id of the first auxiliary constraint that do not need to be
        # checked during dominance breaking.
        self.firstDomInvisible = 1

        self.transitivity = TransitivityInfo()
        self.irreflexivityProven = False

    def check(self):
        if not self.irreflexivityProven:
            raise InvalidProof("Proof did not show irreflexivity of order.")
        if not self.transitivity.isProven:
            raise InvalidProof("Proof did not show transitivity of order.")

class OrderContext:
    @classmethod
    def setup(cls, context):
        try:
            return context.orderContext
        except AttributeError:
            context.orderContext = cls(context)
            return context.orderContext

    def __init__(self, context):
        self.orders = dict()

        self.emptyOrder = Order()
        self.orders[self.emptyOrder.name] = \
            self.emptyOrder
        self.resetToEmptyOrder()

        self.activeDefinition = None

    def resetToEmptyOrder(self):
        self.activeOrder = self.emptyOrder

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

@register_rule
class LoadOrder(EmptyRule):
    Id = "load_order"

    @classmethod
    def parse(cls, line, context):
        orderContext = OrderContext.setup(context)

        with WordParser(line) as words:
            try:
                name = next(words)
            except StopIteration:
                orderContext.resetToEmptyOrder()
                return cls()

            lits = []
            for nxt in words:
                lit = context.ineqFactory.lit2int(nxt)

                if lit < 0:
                    raise ValueError("The order variables"
                        "may not be negated variables.")

                lits.append(lit)

        try:
            order = orderContext.orders[name]
        except KeyError:
            raise ValueError("Unkown order.")

        order.vars = lits
        order.firstDomInvisible = context.firstFreeId

        if len(order.vars) != len(order.leftVars):
            raise ValueError(
                "Order does not specify right number of"
                "variables. Expected: %i, Got: %i"
                % (len(order.vars), len(order.leftVars)))

        orderContext.activeOrder = order

        return cls()

def autoProof(context, db, subgoals, upTo = None):
    if not subgoals:
        return

    verbose = context.verifierSettings.trace

    context.propEngine.increaseNumVarsTo(context.ineqFactory.numVars())
    propEngine = context.propEngine

    assignment = Substitution()
    assignment.addConstants(propEngine.propagatedLits())
    if verbose:
        print("    propagations:", end = " ")
        for i in assignment.constants:
            print(context.ineqFactory.int2lit(i), end = " ")
        print()

    db = {Id: ineq.copy().substitute(*assignment.get()) for Id, ineq in db}
    while subgoals:
        nxtGoalId, nxtGoal = subgoals[0]
        if upTo is not None and nxtGoalId >= upTo:
            break
        else:
            subgoals.popleft()

        nxtGoal = nxtGoal.substitute(*assignment.get())

        if nxtGoalId in db:
            if db[nxtGoalId].implies(nxtGoal):
                if verbose:
                    print("    automatically proved %03i by self implication" % (nxtGoalId))
                continue

        success = nxtGoal.ratCheck([], propEngine)
        if success:
            if verbose:
                print("    automatically proved %03i by RUP check" % (nxtGoalId))
            continue

        for ineqId, ineq in db.items():
            if ineq.implies(nxtGoal):
                success = True
                break

        if success:
            if verbose:
                print("    automatically proved %03i by implication from %i" % (nxtGoalId, ineqId))
            continue

        raise InvalidProof("Could not proof proof goal %03i automatically." % (nxtGoalId))




class EndOfProof(EmptyRule):
    Id = "qed"

    @classmethod
    def parse(cls, line, context):
        subcontexts = SubContext.setup(context)
        subcontext = subcontexts.pop(checkSubgoals = False)
        return cls(subcontext)

    def __init__(self, subcontext):
        self.subcontext = subcontext

    def deleteConstraints(self):
        return self.subcontext.toDelete

    def compute(self, antecedents, context = None):
        autoProof(context, antecedents, self.subcontext.subgoals)
        return self.subcontext.toAdd

    def antecedentIDs(self):
        return "all"

    def allowedRules(self, context, currentRules):
        if self.subcontext.previousRules is not None:
            return self.subcontext.previousRules
        else:
            return currentRules

    def numConstraints(self):
        return len(self.subcontext.toAdd)

class SubProof(EmptyRule):
    Id = "proofgoal"
    subRules = dom_friendly_rules() + [EndOfProof]

    # todo enforce only one def

    @classmethod
    def parse(cls, line, context):
        subcontexts = SubContext.setup(context)
        parentCtx = subcontexts.getCurrent()
        subContext = subcontexts.push()

        if not parentCtx.subgoals:
            raise ValueError("No proofgoals left to proof.")

        nxtGoalId, nxtGoal = parentCtx.subgoals[0]

        with WordParser(line) as words:
            myGoal = words.nextInt()

        if myGoal < nxtGoalId:
            raise ValueError("Invalid subgoal.")

        return cls(subContext, parentCtx.subgoals, myGoal)



    def __init__(self, subContext, subgoals, myGoal):
        self.subContext = subContext
        self.subgoals = subgoals
        self.myGoal = myGoal

        f = lambda context, subContext: self.check(context)
        subContext.callbacks.append(f)



    def compute(self, antecedents, context):
        autoProof(context, antecedents, self.subgoals, self.myGoal)
        nxtGoalId, constraint = self.subgoals.popleft()
        assert(nxtGoalId == self.myGoal)
        return [constraint.copy().negated()]

    def antecedentIDs(self):
        return "all"

    def numConstraints(self):
        return 1

    def allowedRules(self, context, currentRules):
        self.subContext.previousRules = currentRules
        return rulesToDict(self.subRules)

    def check(self, context):
        if not getattr(context, "containsContradiction", False):
            raise InvalidProof("Sub proof did not show contradiction.")

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
    subRules = dom_friendly_rules() + [EndOfProof]

    @classmethod
    def parse(cls, line, context):
        subcontexts = SubContext.setup(context)
        subcontext = subcontexts.push()

        orderContext = OrderContext.setup(context)
        order = orderContext.activeDefinition

        return cls(subcontext, order)

    def __init__(self, subContext, order):
        # todo: don't allow the use of outside constraints

        self.subContext = subContext


        order.irreflexivityProven = True

        self.constraints = []

        mapping = Substitution()
        mapping.mapAll(order.rightVars,order.leftVars)

        for ineq in order.definition:
            ineq = ineq.copy()

            ineq.substitute(*mapping.get())
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

class MultiGoalRule(EmptyRule):
    subRules = [EndOfProof, SubProof]

    def __init__(self, context):
        subContexts = SubContext.setup(context)
        self.subContext = subContexts.push()
        self.displayGoals = context.verifierSettings.trace
        self.constraints = []
        self.ineqFactory = context.ineqFactory
        self.nextId = context.firstFreeId

    def addSubgoal(self, ineq, Id = None):
        if Id is None:
            # block of a new Id for subgoal
            Id = self.addAvailable(None)

        self.subContext.subgoals.append((Id, ineq))
        if self.displayGoals:
            ineqStr = self.ineqFactory.toString(ineq)
            print("  proofgoal %03i: %s"%(Id, ineqStr))

    def addAvailable(self, ineq):
        """add constraint available in sub proof"""
        Id = self.nextId
        self.constraints.append(ineq)

        self.nextId += 1
        return Id

    def addIntroduced(self, ineq):
        """add constraint introduced after all subgoals are proven"""
        self.subContext.toAdd.append(ineq)

    def compute(self, antecedents, context = None):
        return self.constraints

    def numConstraints(self):
        return len(self.constraints)

    def allowedRules(self, context, currentRules):
        self.subContext.previousRules = currentRules
        return rulesToDict(self.subRules)

class TransitivityProof(MultiGoalRule):
    Id = "proof"
    subRules = [EndOfProof, ReversePolishNotation, SubProof]

    @classmethod
    def parse(cls, line, context):
        orderContext = OrderContext.setup(context)
        order = orderContext.activeDefinition

        # This rule will fail if the proof is not correct so lets
        # already mark it proven here.
        order.transitivity.isProven = True

        return cls(context, order)

    def __init__(self, context, order):
        super().__init__(context)

        for ineq in order.definition:
            ineq = ineq.copy()
            self.addAvailable(ineq)

        substitution = Substitution()
        substitution.mapAll(order.leftVars,order.rightVars)
        substitution.mapAll(order.rightVars,order.transitivity.fresh_right)

        for ineq in order.definition:
            ineq = ineq.copy()

            ineq.substitute(*substitution.get())
            self.addAvailable(ineq)


        substitution = Substitution()
        substitution.mapAll(order.rightVars,order.transitivity.fresh_right)

        for ineq in order.definition:
            ineq = ineq.copy()

            ineq.substitute(*substitution.get())

            self.addSubgoal(ineq)

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
        if not self.order.transitivity.isProven:
            raise InvalidProof("Transitivity proof is missing.")

        # todo: don't allow the use of outside constraints


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

class Substitution:
    def __init__(self):
        self.constants = []
        self.substitutions = []
        self.isSorted = True

    def mapAll(self, variables, substitutes):
        for variable, substitute in zip(variables, substitutes):
            self.map(variable, substitute)

    def addConstants(self, constants):
        self.constants.extend(constants)

    def map(self, variable, substitute):
        self.isSorted = False

        if variable < 0:
            raise ValueError("Substitution should only map variables.")

        if substitute is False:
            self.constants.append(-variable)
        elif substitute is True:
            self.constants.append(variable)
        else:
            self.substitutions.append((variable,substitute))

    def get(self):
        if not self.isSorted:
            self.sort()

        if len(self.substitutions) > 0:
            frm, to = zip(*self.substitutions)
        else:
            frm = []
            to = []

        return (self.constants, frm, to)

    def asDict(self):
        res = dict()
        for var, value in self.substitutions:
            res[var] = value
            res[-var] = -value

        for lit in self.constants:
            res[lit] = True
            res[-lit] = False

        return res

    @classmethod
    def fromDict(cls, sub):
        res = Substitution()
        for key, value in sub.items():
            res.map(key, value)
        res.sort()
        return res

    def sort(self):
        self.substitutions.sort(key = lambda x: x[0])
        self.isSorted = True

    @classmethod
    def parse(cls, words, ineqFactory, forbidden = []):
        result = cls()

        try:
            nxt = next(words)
        except StopIteration:
            raise ValueError("Expected substitution found nothing.")

        while nxt != ";":
            frm = ineqFactory.lit2int(nxt)
            if frm < 0:
                raise ValueError("Substitution should only"
                    "map variables, not negated literals.")
            if frm in forbidden:
                raise ValueError("Substitution contains forbidden variable.")

            try:
                nxt = next(words)
                if nxt == "→" or nxt == "->":
                    nxt = next(words)
            except StopIteration:
                raise ValueError("Substitution is missing"
                    "a value for the last variable.")

            if nxt == "0":
                to = False
            elif nxt == "1":
                to = True
            else:
                to  = ineqFactory.lit2int(nxt)

            result.map(frm,to)

            try:
                nxt = next(words)

                if nxt == ",":
                    nxt = next(words)
            except StopIteration:
                break

        result.sort()
        return result


def objectiveCondition(context, witnessDict):
    if getattr(context, "objective", None) is None:
        return None

    # obj(x\omega) \leq obj(x)
    terms = []
    degree = 0

    for lit, coeff in context.objective.items():
        if lit in witnessDict:
            # note that literals that are not remaped will disapear
            # anyway, so no reason to add them
            terms.append((coeff,lit))

            lit = witnessDict[lit]

            if lit is True:
                degree += coeff
            elif lit is not False:
                terms.append((-coeff,lit))

    return context.ineqFactory.fromTerms(terms, degree)

@register_rule
class MapRedundancy(MultiGoalRule):
    Id = "map"

    @classmethod
    def parse(cls, line, context):
        orderContext = OrderContext.setup(context)
        order = orderContext.activeOrder

        with WordParser(line) as words:
            substitution = Substitution.parse(
                words = words,
                ineqFactory = context.ineqFactory,
                forbidden = order.vars)

            parser = OPBParser(
                ineqFactory = context.ineqFactory,
                allowEq = False)
            ineq = parser.parseConstraint(words)

        return cls(context, ineq[0], substitution)

    def __init__(self, context, constraint, witness):
        super().__init__(context)

        self.constraint = constraint
        self.witness = witness

        self.addIntroduced(constraint)

    def antecedentIDs(self):
        return "all"

    #@TimedFunction.time("MapRedundancy.compute")
    def compute(self, antecedents, context):
        ineq = self.constraint.copy()
        ineq = ineq.negated()
        self.addAvailable(ineq)

        for Id, ineq in antecedents:
            rhs = ineq.copy()
            rhs.substitute(*self.witness.get())
            if rhs != ineq:
                self.addSubgoal(rhs, Id)

        ineq = self.constraint.copy()
        ineq.substitute(*self.witness.get())
        self.addSubgoal(ineq)

        obj = objectiveCondition(context, self.witness.asDict())
        if obj is not None:
            self.addSubgoal(obj)

        return super().compute(antecedents, context)


@register_rule
class DominanceRule(MultiGoalRule):
    Id = "dom"

    @classmethod
    def parse(cls, line, context):
        orderContext = OrderContext.setup(context)
        order = orderContext.activeOrder

        with WordParser(line) as words:
            substitution = Substitution.parse(
                words = words,
                ineqFactory = context.ineqFactory)

            parser = OPBParser(
                ineqFactory = context.ineqFactory,
                allowEq = False)
            ineq = parser.parseConstraint(words)

        return cls(context, ineq[0], substitution, order)

    def __init__(self, context, constraint, witness, order):
        super().__init__(context)

        self.constraint = constraint
        self.witness = witness
        self.order = order

        self.addIntroduced(constraint)

    def antecedentIDs(self):
        return "all"

    #@TimedFunction.time("MapRedundancy.compute")
    def compute(self, antecedents, context):
        ineq = self.constraint.copy()
        ineq = ineq.negated()
        self.addAvailable(ineq)

        for Id, ineq in antecedents:
            if Id >= self.order.firstDomInvisible:
                break

            rhs = ineq.copy()
            rhs.substitute(*self.witness.get())
            if rhs != ineq:
                self.addSubgoal(rhs, Id)


        witnessDict = self.witness.asDict()

        zippedVars = zip(
            self.order.leftVars,
            self.order.rightVars,
            self.order.vars)

        mapping = dict()
        for leftVar, rightVar, var in zippedVars:
            mapping[leftVar] = witnessDict[var]
            mapping[rightVar] = var

        for key, value in witnessDict.items():
            if key in self.order.auxVars:
                mapping[key] = value

        substitution = Substitution.fromDict(mapping)

        # todo: we need to check that the auxVars are still fresh

        # for ineq in self.order.auxDefinition:
        #     ineq = self.constraint.copy()
        #     ineq.substitute(*substitution.get())
        #     self.addAvailable(ineq)

        for ineq in self.order.definition:
            ineq = ineq.copy()
            ineq.substitute(*substitution.get())
            self.addSubgoal(ineq)

        obj = objectiveCondition(context, witnessDict)
        if obj is not None:
            self.addSubgoal(obj)

        return super().compute(antecedents, context)