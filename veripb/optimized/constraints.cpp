#ifdef PY_BINDINGS
    #include <pybind11/pybind11.h>
    #include <pybind11/stl.h>
    #include <pybind11/iostream.h>
    #include <pybind11/functional.h>
    namespace py = pybind11;
#endif

#include "constraints.hpp"

void ClausePropagator::propagate() {
    while (current.qhead < trail.size() and !current.conflict) {
        Lit falsifiedLit = ~trail[current.qhead];
        // std::cout << "propagating: " << trail[current.qhead] << std::endl;

        WatchList& ws = watchlist[falsifiedLit];

        const WatchedType* end = ws.data() + ws.size();
        WatchedType* sat = ws.data();
        // WatchedType* it  = ws.data();
        // for (; it != end; it++) {
        //     if (assignment.value[it->other] == State::True) {
        //         std::swap(*it, *sat);
        //         ++sat;
        //     }
        // }


        WatchedType* next = &(*sat);
        WatchedType* kept = next;

        const uint lookAhead = 3;
        for (; next != end && !current.conflict; next++) {
            auto fetch = next + lookAhead;
            if (fetch < end) {
                __builtin_prefetch(fetch->ineq);
            }
            assert(next->other != Lit::Undef() || assignment.value[next->other] == State::Unassigned);

            bool keepWatch = true;
            if (assignment.value[next->other] != State::True) {
                keepWatch = next->ineq->updateWatch(*this, falsifiedLit, false);
            }
            if (keepWatch) {
                *kept = *next;
                kept += 1;
            }
        }

        // in case of conflict copy remaining watches
        for (; next != end; next++) {
            *kept = *next;
            kept += 1;
        }

        ws.erase(ws.begin() + (kept - ws.data()), ws.end());

        current.qhead += 1;
    }
}

int main(int argc, char const *argv[])
{
    /* code */
    {
    Inequality<CoefType> foo({1,1,1},{1,1,1},1);
    Inequality<CoefType> baa({1,1,1},{-1,-1,-1},3);
    PropEngine<CoefType> p(10);
    foo.eq(&baa);
    foo.implies(&baa);
    foo.negated();
    Inequality<CoefType> test(baa);
    p.attach(&foo, 1);
    }
    Inequality<CoefType> foo({1,1,1},{1,1,1},1);

    std::cout << foo.isContradiction() << std::endl;

    std::cout << std::endl;
    std::cout << (~Lit(Var(2), false)).var() << std::endl;
    int test = 4 ^ 1;
    std::cout << test << std::endl;
    return 0;
}

int hashColision = 0;

#ifdef PY_BINDINGS
    void init_constraints(py::module &m){
        m.doc() = "Efficient implementation for linear combinations of constraints.";
        m.def("maxId", []() { return std::numeric_limits<uint64_t>::max(); });

        py::class_<Substitution>(m, "Substitution")
            .def(py::init<std::vector<int>&,std::vector<int>&,std::vector<int>&>());

        py::class_<PropEngine<CoefType>>(m, "PropEngine")
            .def(py::init<size_t>())
            .def("attach", &PropEngine<CoefType>::attach)
            .def("detach", &PropEngine<CoefType>::detach)
            .def("getDeletions", &PropEngine<CoefType>::getDeletions)
            .def("attachCount", &PropEngine<CoefType>::attachCount)
            .def("reset", &PropEngine<CoefType>::reset)
            .def("checkSat", &PropEngine<CoefType>::checkSat)
            .def("propagatedLits", &PropEngine<CoefType>::propagatedLits)
            .def("increaseNumVarsTo", &PropEngine<CoefType>::increaseNumVarsTo)
            .def("printStats", &PropEngine<CoefType>::printStats)
            .def("computeEffected", &PropEngine<CoefType>::computeEffected)
            .def("estimateNumEffected", &PropEngine<CoefType>::estimateNumEffected)
            .def("find", &PropEngine<CoefType>::find);

        auto cppIneq = py::class_<Inequality<CoefType>>(m, "CppInequality")
            .def(py::init<std::vector<CoefType>&, std::vector<int>&, CoefType>())
            .def_readonly("minId", &Inequality<CoefType>::minId)
            .def("saturate", &Inequality<CoefType>::saturate)
            .def("divide", &Inequality<CoefType>::divide)
            .def("multiply", &Inequality<CoefType>::multiply)
            .def("add", &Inequality<CoefType>::add)
            .def("contract", &Inequality<CoefType>::contract)
            .def("copy", &Inequality<CoefType>::copy)
            .def("implies", &Inequality<CoefType>::implies)
            .def("expand", &Inequality<CoefType>::expand)
            .def("negated", &Inequality<CoefType>::negated)
            .def("rupCheck", &Inequality<CoefType>::rupCheck)
            .def("__eq__", &Inequality<CoefType>::eq)
            .def("__repr__", &Inequality<CoefType>::repr)
            .def("toString", &Inequality<CoefType>::toString)
            .def("toOPB", &Inequality<CoefType>::repr)
            .def("isContradiction", &Inequality<CoefType>::isContradiction)
            .def("substitute", &Inequality<CoefType>::substitute)
            .def("weaken", &Inequality<CoefType>::weaken);
        cppIneq.attr("__hash__") = py::none();

        py::class_<PropEngine<BigInt>>(m, "PropEngineBigInt")
            .def(py::init<size_t>())
            .def("attach", &PropEngine<BigInt>::attach)
            .def("detach", &PropEngine<BigInt>::detach)
            .def("getDeletions", &PropEngine<BigInt>::getDeletions)
            .def("attachCount", &PropEngine<BigInt>::attachCount)
            .def("reset", &PropEngine<BigInt>::reset)
            .def("checkSat", &PropEngine<BigInt>::checkSat)
            .def("propagatedLits", &PropEngine<BigInt>::propagatedLits)
            .def("increaseNumVarsTo", &PropEngine<BigInt>::increaseNumVarsTo)
            .def("printStats", &PropEngine<BigInt>::printStats)
            .def("computeEffected", &PropEngine<BigInt>::computeEffected)
            .def("estimateNumEffected", &PropEngine<BigInt>::estimateNumEffected)
            .def("find", &PropEngine<BigInt>::find);


        auto cppIneqBigInt = py::class_<Inequality<BigInt>>(m, "CppInequalityBigInt")
            .def(py::init<std::vector<BigInt>&, std::vector<int>&, BigInt>())
            .def_readonly("minId", &Inequality<BigInt>::minId)
            .def("saturate", &Inequality<BigInt>::saturate)
            .def("divide", &Inequality<BigInt>::divide)
            .def("multiply", &Inequality<BigInt>::multiply)
            .def("add", &Inequality<BigInt>::add)
            .def("contract", &Inequality<BigInt>::contract)
            .def("copy", &Inequality<BigInt>::copy)
            .def("implies", &Inequality<BigInt>::implies)
            .def("expand", &Inequality<BigInt>::expand)
            .def("negated", &Inequality<BigInt>::negated)
            .def("rupCheck", &Inequality<BigInt>::rupCheck)
            .def("__eq__", &Inequality<BigInt>::eq)
            .def("__repr__", &Inequality<BigInt>::repr)
            .def("toString", &Inequality<BigInt>::toString)
            .def("toOPB", &Inequality<BigInt>::repr)
            .def("isContradiction", &Inequality<BigInt>::isContradiction)
            .def("substitute", &Inequality<BigInt>::substitute)
            .def("weaken", &Inequality<BigInt>::weaken);
        cppIneqBigInt.attr("__hash__") = py::none();

    }
#endif
