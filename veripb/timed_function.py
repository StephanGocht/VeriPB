import logging

from time import perf_counter
from collections import defaultdict

class TimedFunction:
    times = defaultdict(int)

    @classmethod
    def startTotalTimer(cls):
        cls.start = perf_counter()

    @classmethod
    def addTime(cls, name, time):
        cls.times[name] += time

    @classmethod
    def time(cls, name):
        def wrapp(function):
            def wrapper(*args, **kwargs):
                start = perf_counter()
                res = function(*args, **kwargs)
                cls.addTime(name, perf_counter() - start)
                return res

            return wrapper
        return wrapp

    @classmethod
    def timeIter(cls, name):
        def wrapp(function):
            def wrapper(*args, **kwargs):
                it = function(*args, **kwargs)

                try:
                    while True:
                        start = perf_counter()
                        value = next(it)
                        cls.addTime(name, perf_counter() - start)
                        yield value
                except StopIteration:
                    pass

            return wrapper
        return wrapp

    @classmethod
    def print_stats(cls):
        total = 0
        for name, time in cls.times.items():
            total += time
            logging.info("time in %s: %.2fs"%(name,time))

        logging.info("sum time: %.2fs"%(total))
        logging.info("total time: %.2fs"%(perf_counter() - cls.start))