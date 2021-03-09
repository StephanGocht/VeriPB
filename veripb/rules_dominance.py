from veripb import InvalidProof
from veripb.rules import Rule, EmptyRule, register_rule
from veripb.rules import ReversePolishNotation, IsContradiction
from veripb.rules_register import register_rule, dom_friendly_rules, rules_to_dict
from veripb.parser import OPBParser, WordParser, ParseContext

from veripb.optimized.constraints import maxId as getMaxConstraintId
constraintMaxId = getMaxConstraintId()

from veripb import verifier

from veripb.timed_function import TimedFunction

from collections import deque

from veripb.rules_multigoal import *
from veripb.autoproving import *

from collections import defaultdict



#@TimedFunction.time("computeEffected")
def computeEffected(context, substitution, maxId = constraintMaxId):
    return context.propEngine.computeEffected(substitution, maxId)

class TransitivityInfo:
    def __init__(self):
        self.fresh_right = []
        self.fresh_aux_1 = []
        self.fresh_aux_2 = []
        self.isProven = False

class GoalCache:
    def __init__(self):
        self.goals = None


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
        self.vars = None

        # Id of the first auxiliary constraint that do not need to be
        # checked during dominance breaking.
        self.firstDomInvisible = None

        self.transitivity = TransitivityInfo()
        self.irreflexivityProven = False

        self.goalCache = defaultdict(GoalCache)

    def check(self):
        if not self.irreflexivityProven:
            raise InvalidProof("Proof did not show irreflexivity of order.")
        if not self.transitivity.isProven:
            raise InvalidProof("Proof did not show transitivity of order.")

    def reset(self):
        self.goalCache = defaultdict(GoalCache)
        self.firstDomInvisible = None
        self.vars = None

    def getCachedGoals(self, context, witness):
        cache = self.goalCache[witness]
        if cache.goals is None:
            cache.goals = computeEffected(context, witness.get(), self.firstDomInvisible)

            witnessDict = witness.asDict()

            zippedVars = zip(
                self.leftVars,
                self.rightVars,
                self.vars)

            substitution = Substitution()
            for leftVar, rightVar, var in zippedVars:
                try:
                    substitution.map(leftVar, witnessDict[var])
                except KeyError:
                    substitution.map(leftVar, var)
                substitution.map(rightVar, var)

            for var in self.auxVars:
                try:
                    substitution.map(var, witnessDict[var])
                except KeyError:
                    pass

            # # todo: we need to check that the auxVars are still fresh
            sub = substitution.get()

            # for ineq in self.order.auxDefinition:
            #     ineq = self.constraint.copy()
            #     ineq.substitute(sub)
            #     self.addAvailable(ineq)

            for ineq in self.definition:
                ineq = ineq.copy()
                ineq.substitute(sub)
                cache.goals.append(ineq)

            obj = objectiveCondition(context, witnessDict)
            if obj is not None:
                cache.goals.append(obj)

        return cache.goals

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
        self.activeOrder = None
        self.resetToEmptyOrder()

        self.activeDefinition = None

    def resetToEmptyOrder(self):
        self.activateOrder(self.emptyOrder, [], 1)

    def activateOrder(self, order, lits, firstDomInvisible):
        if self.activeOrder is not None:
            self.activeOrder.reset()

        order.vars = lits
        order.firstDomInvisible = firstDomInvisible
        self.activeOrder = order

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
    Ids = ["load_order"]

    # todo: allow negated literals

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
                raise ValueError("Unkown order '%s'." %(name))

        if len(lits) != len(order.leftVars):
            raise ValueError(
                "Order does not specify right number of"
                "variables. Expected: %i, Got: %i"
                % (len(order.vars), len(order.leftVars)))

        orderContext.activateOrder(order, lits, context.firstFreeId)

        return cls()

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

        context.propEngine.increaseNumVarsTo(context.ineqFactory.numVars())

        order = OrderContext.setup(context)
        cls.addLits(order.activeDefinition, lits)
        return cls()


class LeftVars(OrderVarsBase):
    Ids = ["left"]

    @classmethod
    def addLits(cls, order, lits):
        order.leftVars.extend(lits)

class RightVars(OrderVarsBase):
    Ids = ["right"]

    @classmethod
    def addLits(cls, order, lits):
        order.rightVars.extend(lits)

class AuxVars(OrderVarsBase):
    Ids = ["aux"]

    @classmethod
    def addLits(cls, order, lits):
        order.auxVars.extend(lits)

class OrderVars(EmptyRule):
    # todo: add check that number of variables matches

    Ids = ["vars"]
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
        return rules_to_dict(self.subRules)

class OrderDefinition(EmptyRule):
    Ids = [""]

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
    Ids = ["def"]
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
        return rules_to_dict(self.subRules)

class Irreflexivity(MultiGoalRule):
    Ids = ["irreflexive"]
    subRules = dom_friendly_rules() + [EndOfProof, SubProof]

    @classmethod
    def parse(cls, line, context):
        orderContext = OrderContext.setup(context)
        order = orderContext.activeDefinition

        return cls(context, order)

    def __init__(self, context, order):
        super().__init__(context)

        order.irreflexivityProven = True

        mapping = Substitution()
        mapping.mapAll(order.rightVars,order.leftVars)

        sub = mapping.get()
        for ineq in order.definition:
            ineq = ineq.copy()

            ineq.substitute(sub)
            self.addAvailable(ineq)

        contradiction = context.ineqFactory.fromTerms([], 1)
        self.addSubgoal(contradiction)


class TransitivityFreshRight(OrderVarsBase):
    Ids = ["fresh_right"]

    @classmethod
    def addLits(cls, order, lits):
        order.transitivity.fresh_right.extend(lits)


class TransitivityFreshAux1(OrderVarsBase):
    Ids = ["fresh_aux1"]

    @classmethod
    def addLits(cls, order, lits):
        order.transitivity.fresh_aux_1.extend(lits)

class TransitivityFreshAux2(OrderVarsBase):
    Ids = ["fresh_aux2"]

    @classmethod
    def addLits(cls, order, lits):
        order.transitivity.fresh_aux_2.extend(lits)


class TransitivityVars(EmptyRule):
    # todo: add check that number of variables matches

    Ids = ["vars"]
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
        return rules_to_dict(self.subRules)

class TransitivityProof(MultiGoalRule):
    Ids = ["proof"]
    subRules = dom_friendly_rules() + [EndOfProof, SubProof]

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

        sub =  substitution.get()
        for ineq in order.definition:
            ineq = ineq.copy()

            ineq.substitute(sub)
            self.addAvailable(ineq)


        substitution = Substitution()
        substitution.mapAll(order.rightVars,order.transitivity.fresh_right)

        sub = substitution.get()
        for ineq in order.definition:
            ineq = ineq.copy()

            ineq.substitute(sub)

            self.addSubgoal(ineq)

class Transitivity(EmptyRule):
    Ids = ["transitivity"]
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


    def allowedRules(self, context, currentRules):
        self.subContext.previousRules = currentRules
        return rules_to_dict(self.subRules)

class StopSubVerifier(Exception):
    pass

class SubVerifierEnd(EmptyRule):
    Ids = ["qed", "end"]

    def __init__(self, oldParseContext):
        self.oldParseContext = oldParseContext

    def compute(self, antecedents, context):
        orders = OrderContext.setup(context)
        orders.qedNewOrder()
        raise StopSubVerifier()

    def newParseContext(self, parseContext):
        return self.oldParseContext

class SubVerifierEndRule():
    def __init__(self, oldParseContext):
        self.oldParseContext = oldParseContext

    def parse(self, line, context):
        return SubVerifierEnd(self.oldParseContext)


class SubVerifier(EmptyRule):
    def compute(self, antecedents, context):
        # context for sub verifier
        svContext = verifier.Context()
        svContext.newPropEngine = context.newPropEngine
        svContext.propEngine = svContext.newPropEngine()
        svContext.ineqFactory = context.ineqFactory
        svContext.ruleCount = getattr(context, "ruleCount", 0)

        self._newParseContext = ParseContext(svContext)


        self.onEnterSubVerifier(context, svContext)

        try:
            verify = verifier.Verifier(
                context = svContext,
                settings = context.verifierSettings)
            verify(context.rules)
        except StopSubVerifier:
            self.exitSubVerifier(context, svContext)
        else:
            raise InvalidProof("Subproof not finished.")

        return []

    def exitSubVerifier(self, context, subVerifierContext):
        context.usesAssumptions = getattr(subVerifierContext, "usesAssumptions", False)
        self.onExitSubVerifier(context, subVerifierContext)

    def onEnterSubVerifier(self, context, subVerifierContext):
        pass

    def onExitSubVerifier(self, context, subVerifierContext):
        pass

    def newParseContext(self, parseContext):
        self._newParseContext.rules = \
            self.allowedRules(
                parseContext.context,
                parseContext.rules)

        rule = SubVerifierEndRule(parseContext)
        for Id in SubVerifierEnd.Ids:
            self._newParseContext.rules[Id] = rule

        return self._newParseContext

@register_rule
class StrictOrder(SubVerifier):
    Ids = ["strict_order"]
    subRules = [OrderVars, OrderDefinitions, Irreflexivity, Transitivity]

    @classmethod
    def parse(cls, line, context):
        with WordParser(line) as words:
            name = next(words)

        return cls(name)

    def __init__(self, name):
        self.name = name

    def onEnterSubVerifier(self, context, subContext):
        orders = OrderContext.setup(subContext)
        orders.newOrder(self.name)

    def onExitSubVerifier(self, context, subContext):
        orderSubContext = OrderContext.setup(subContext)
        orderContext = OrderContext.setup(context)
        orderContext.orders[self.name] = orderSubContext.orders[self.name]

    def allowedRules(self, context, currentRules):
        return rules_to_dict(self.subRules)



def objectiveCondition(context, witnessDict):
    if getattr(context, "objective", None) is None:
        return None

    # obj(x\omega) \leq obj(x)
    terms = []
    degree = 0

    for lit, coeff in context.objective.items():
        try:
            lit2 = witnessDict[lit]

            terms.append((coeff,lit))

            if lit2 is True:
                degree += coeff
            elif lit2 is not False:
                terms.append((-coeff,lit))

        except KeyError:
            # note that literals that are not remaped will disapear
            # anyway, so no reason to add them
            pass

    return context.ineqFactory.fromTerms(terms, degree)


class Stats:
    def __init__(self):
        self.numGoalCandidates = 0
        self.numSubgoals = 0

    def print_stats(self):
        for attr in dir(self):
            value = getattr(self, attr)
            if not callable(value) and not attr.startswith("__"):
                print("c statistic: %s: %s"%(attr, str(value)))

stats = Stats()



@register_rule
class AddRedundant(MultiGoalRule):
    Ids = ["red"]

    @classmethod
    def parse(cls, line, context):
        orderContext = OrderContext.setup(context)
        order = orderContext.activeOrder

        with WordParser(line) as words:
            parser = OPBParser(
                ineqFactory = context.ineqFactory,
                allowEq = False)
            ineq = parser.parseConstraint(words)

            substitution = Substitution.parse(
                words = words,
                ineqFactory = context.ineqFactory,
                forbidden = order.vars)

            context.propEngine.increaseNumVarsTo(context.ineqFactory.numVars())

            autoProveAll = not cls.parseHasExplicitSubproof(words)

        return cls(context, ineq[0], substitution, autoProveAll)

    def __init__(self, context, constraint, witness, autoProveAll):
        super().__init__(context)

        self.constraint = constraint
        self.witness = witness

        self.autoProveAll = autoProveAll

        self.addIntroduced(constraint)

    def antecedentIDs(self):
        return "all"

    @TimedFunction.time("Redundant.compute")
    def compute(self, antecedents, context):
        ineq = self.constraint.copy()

        if self.autoProveAll:
            rup = ineq.rupCheck(context.propEngine)
            if rup:
                self.autoProof(context, antecedents)
                return super().compute(antecedents, context)

        negated = ineq.negated()
        self.addAvailable(negated)

        witness = self.witness.get()
        effected = computeEffected(context, witness)
        for ineq in effected:
            stats.numGoalCandidates += 1
            if not negated.implies(ineq):
                stats.numSubgoals += 1
                assert(ineq.minId != 0)
                self.addSubgoal(ineq, ineq.minId)

        ineq = self.constraint.copy()
        ineq.substitute(witness)
        self.addSubgoal(ineq)

        obj = objectiveCondition(context, self.witness.asDict())
        if obj is not None:
            self.addSubgoal(obj)

        if self.autoProveAll:
            self.autoProof(context, antecedents)
        return super().compute(antecedents, context)


@register_rule
class DominanceRule(MultiGoalRule):
    Ids = ["dom"]

    @classmethod
    def parse(cls, line, context):
        orderContext = OrderContext.setup(context)
        order = orderContext.activeOrder

        with WordParser(line) as words:
            parser = OPBParser(
                ineqFactory = context.ineqFactory,
                allowEq = False)
            ineq = parser.parseConstraint(words)

            substitution = Substitution.parse(
                words = words,
                ineqFactory = context.ineqFactory)

            context.propEngine.increaseNumVarsTo(context.ineqFactory.numVars())

            autoProveAll = not cls.parseHasExplicitSubproof(words)

        return cls(context, ineq[0], substitution, order, autoProveAll)

    def __init__(self, context, constraint, witness, order, autoProveAll):
        super().__init__(context)

        self.constraint = constraint
        self.witness = witness
        self.order = order

        self.autoProveAll = autoProveAll

        self.addIntroduced(constraint)

    def antecedentIDs(self):
        return "all"

    @TimedFunction.time("DominanceRule.compute")
    def compute(self, antecedents, context):
        ineq = self.constraint.copy()
        ineq = ineq.negated()
        self.addAvailable(ineq)

        effected = self.order.getCachedGoals(context, self.witness)

        for ineq in effected:
            assert(ineq.minId != 0)
            self.addSubgoal(ineq, ineq.minId)

        if self.autoProveAll:
            self.autoProof(context, antecedents)

        return super().compute(antecedents, context)