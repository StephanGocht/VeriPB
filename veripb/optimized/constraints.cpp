#ifdef PY_BINDINGS
    #include <pybind11/pybind11.h>
    #include <pybind11/stl.h>
    #include <pybind11/iostream.h>
    #include <pybind11/functional.h>
    namespace py = pybind11;
#endif

#include "constraints.h"
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
//     p.attach(&foo);
//     p.attachTmp(&baa);
//     }
//     Inequality<CoefType> foo({1,1,1},{1,1,1},1);

//     std::cout << foo.isContradiction() << std::endl;

//     std::cout << std::endl;
//     std::cout << (~Lit(Var(2), false)).var() << std::endl;
//     int test = 4 ^ 1;
//     std::cout << test << std::endl;
//     return 0;
// }

#ifdef PY_BINDINGS
    void init_constraints(py::module &m){
        m.doc() = "Efficient implementation for linear combinations of constraints.";

        py::class_<PropEngine<CoefType>>(m, "PropEngine")
            .def(py::init<CoefType>())
            .def("attach", &PropEngine<CoefType>::attach)
            .def("detach", &PropEngine<CoefType>::detach)
            .def("reset", &PropEngine<CoefType>::reset)
            .def("checkSat", &PropEngine<CoefType>::checkSat)
            .def("increaseNumVarsTo", &PropEngine<CoefType>::increaseNumVarsTo)
            .def("printStats", &PropEngine<CoefType>::printStats);


        py::class_<Inequality<CoefType>>(m, "CppInequality")
            .def(py::init<std::vector<CoefType>&, std::vector<CoefType>&, CoefType>())
            .def("saturate", &Inequality<CoefType>::saturate)
            .def("divide", &Inequality<CoefType>::divide)
            .def("multiply", &Inequality<CoefType>::multiply)
            .def("add", &Inequality<CoefType>::add)
            .def("contract", &Inequality<CoefType>::contract)
            .def("copy", &Inequality<CoefType>::copy)
            .def("implies", &Inequality<CoefType>::implies)
            .def("expand", &Inequality<CoefType>::expand)
            .def("contract", &Inequality<CoefType>::contract)
            .def("negated", &Inequality<CoefType>::negated)
            .def("ratCheck", &Inequality<CoefType>::ratCheck)
            .def("__eq__", &Inequality<CoefType>::eq)
            .def("__repr__", &Inequality<CoefType>::repr)
            .def("toString", &Inequality<CoefType>::toString)
            .def("toOPB", &Inequality<CoefType>::repr)
            .def("isContradiction", &Inequality<CoefType>::isContradiction);

    }
#endif
