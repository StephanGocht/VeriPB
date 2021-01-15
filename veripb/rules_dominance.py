from veripb import InvalidProof
from veripb.rules import Rule, EmptyRule, register_rule
from veripb.rules import LevelStack
from veripb.rules_register import register_rule


class LevelCallback():
    @classmethod
    def setup(cls, context, namespace = None):
        try:
            return context.levelCallback[namespace]
        except AttributeError:
            context.levelCallback = dict()
            addIndex = True
        except IndexError:
            addIndex = True

        if addIndex:
            levelCallback = cls()
            context.levelCallback[namespace] = levelCallback
            return context.levelCallback[namespace]

    def __init__(self):
        self.currentLevel = 0
        self.levels = list()

    def setLevel(self, level):
        self.currentLevel = level
        while len(self.levels) <= level:
            self.levels.append(list())

    def addToCurrentLevel(self, callback):
        self.levels[self.currentLevel].append(callback)

    def dropToLevel(self, level):
        assert(level < len(self.levels))

        result = list()
        for i in range(level, len(self.levels)):
            result.extend(self.levels[i])
            self.levels[i].clear()

        return result



class Order:
    def __init__(self, name = ""):
        self.name = name
        self.definition = None
        self.auxDefinition = None
        self.leftVars = None
        self.rightVars = None
        self.auxVars = None

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

@register_rule
class EndOfProof(EmptyRule):
    Id = "qed"

    @classmethod
    def parse(cls, line, context):
        levelStack = LevelStack.setup(context, "subproof")
        level = levelStack.currentLevel
        removed = levelStack.wipeLevel(level)
        levelStack.setLevel(level - 1)

        levelCallbacks = LevelCallback.setup(context)
        callbacks = levelCallbacks.dropToLevel(levelCallbacks.currentLevel - 1)

        added = []
        for callback in callbacks:
            added.extend(callback())

        return cls(removed, added)

    def __init__(self, deleteConstraints, added):
        self._deleteConstraints = deleteConstraints
        self.added = added

    def deleteConstraints(self):
        return self._deleteConstraints

    def compute(self, antecedents, context = None):
        return self.added

@register_rule
class StrictOrder(EmptyRule):
    Id = "strict_order"

    @classmethod
    def parse(cls, line, context):
        name = line.strip()
        orders = OrderContext.setup(context)
        orders.newOrder(name)

        levelStack = LevelStack.setup(context, "subproof")
        levelStack.setLevel(levelStack.currentLevel + 1)

        levelCallbacks = LevelCallback.setup(context)
        levelCallbacks.setLevel(levelCallbacks.currentLevel + 1)
        levelCallbacks.addToCurrentLevel(lambda: orders.qedNewOrder())

        return cls()