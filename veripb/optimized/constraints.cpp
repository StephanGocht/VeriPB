#ifdef PY_BINDINGS
    #include <pybind11/pybind11.h>
    #include <pybind11/stl.h>
    #include <pybind11/iostream.h>
    #include <pybind11/functional.h>
    namespace py = pybind11;
#endif

#include "constraints.hpp"


Propagator::Propagator(PropagationMaster& _propMaster)
    : propMaster(_propMaster)
{
    propMaster.addPropagator(*this);
}

long divideAndRoundUp(long value, BigInt divisor) {
    BigInt result = ((value + divisor - 1) / divisor);
    return result.get_si();
}

void ClausePropagator::propagate() {
    // std::cout << "ClausePropagator: propagating from: " << qhead << std::endl;
    // std::cout << *this << std::endl;
    const auto& trail = propMaster.getTrail();
    const Assignment& assignment = propMaster.getAssignment();
    while (qhead < trail.size() and !propMaster.isConflicting()) {
        Lit falsifiedLit = ~trail[qhead];
        // std::cout << "propagating: " << trail[qhead] << std::endl;

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


        WatchedType* next = sat;
        WatchedType* kept = next;

        const uint lookAhead = 3;
        for (; next != end && !propMaster.isConflicting(); next++) {
            auto fetch = next + lookAhead;
            if (fetch < end) {
                __builtin_prefetch(fetch->ineq);
            }
            assert(next->other != Lit::Undef() || assignment[next->other] == State::Unassigned);

            bool keepWatch = true;
            if (assignment.value[next->other] != State::True) {
                assert(next->ineq->header.isMarkedForDeletion == false);
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

        qhead += 1;
    }
}

void ClausePropagator::cleanupWatches() {
    for (WatchList& wl: watchlist) {
        wl.erase(
            std::remove_if(
                wl.begin(),
                wl.end(),
                [](auto& watch){
                    // assert(!watch.ineq->header.isMarkedForDeletion);
                    return watch.ineq->header.isMarkedForDeletion;
                }
            ),
            wl.end()
        );
    }
}

// int main(int argc, char const *argv[])
// {
//     numLexOrder cmp;
//     std::vector<std::string> values = {
//         "x_00",
//         "x_0",
//         "x_123",
//         "x_00123",
//         "x_0123",
//         "y_1",
//         "y_0",
//         "x_123_002",
//         "x_123_001",
//         "x_023_001",
//         "x_23_001"
//     };

//     std::sort(values.begin(),values.end(), cmp);

//     for (auto s : values) {
//         std::cout << s << std::endl;
//     }
//     return 0;
// }

// int main(int argc, char const *argv[])
// {
//     /* code */
//     {
//     Inequality<CoefType> foo({1,1,1},{1,1,1},1);
//     Inequality<CoefType> baa({1,1,1},{-1,-1,-1},3);
//     PropEngine<CoefType> p(10);
//     foo.eq(&baa);
//     foo.implies(&baa);
//     foo.negated();
//     Inequality<CoefType> test(baa);
//     p.attach(&foo, 1);
//     }
//     Inequality<CoefType> foo({1,1,1},{1,1,1},1);

//     std::cout << foo.isContradiction() << std::endl;

//     std::cout << std::endl;
//     std::cout << (~Lit(Var(2), false)).var() << std::endl;
//     int test = 4 ^ 1;
//     std::cout << test << std::endl;
//     return 0;
// }

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
            .def("checkSat", &PropEngine<CoefType>::checkSat)
            .def("propagatedLits", &PropEngine<CoefType>::propagatedLits)
            .def("increaseNumVarsTo", &PropEngine<CoefType>::increaseNumVarsTo)
            .def("printStats", &PropEngine<CoefType>::printStats)
            .def("computeEffected", &PropEngine<CoefType>::computeEffected)
            .def("find", &PropEngine<CoefType>::find)
            .def("moveToCore", &PropEngine<CoefType>::moveToCore)
            .def("moveMultipleToCore", &PropEngine<CoefType>::moveMultipleToCore)
            .def("moveAllToCore", &PropEngine<CoefType>::moveAllToCore);

        py::class_<Assignment>(m, "Assignment")
            .def(py::init<std::vector<int>&>());


        auto cppIneq = py::class_<Inequality<CoefType>>(m, "CppInequality")
            .def(py::init<std::vector<CoefType>&, std::vector<int>&, CoefType>())
            .def_readonly("minId", &Inequality<CoefType>::minId)
            .def_readonly("isCoreConstraint", &Inequality<CoefType>::isCoreConstraint)
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
            .def("isSAT", &Inequality<CoefType>::isSAT)
            .def("isTrivial", &Inequality<CoefType>::isTrivial)
            .def("substitute", &Inequality<CoefType>::substitute)
            .def("weaken", &Inequality<CoefType>::weaken);
        cppIneq.attr("__hash__") = py::none();

    }
#endif
