/*
<%
cfg['compiler_args'] = ['-std=c++17', '-g', '-DPY_BINDINGS']
cfg['dependencies'] = ['constraints.h']
cfg['sources'] = ['constraints.cpp', 'parsing.cpp']
setup_pybind11(cfg)
%>
*/

#ifdef PY_BINDINGS
    #include <pybind11/pybind11.h>
    #include <pybind11/stl.h>
    #include <pybind11/iostream.h>
    #include <pybind11/functional.h>
    namespace py = pybind11;
#endif

// void init_constraints(py::module &);
// void init_parsing(py::module &);

void init_constraints(py::module &);
void init_parsing(py::module &);

#ifdef PY_BINDINGS
    PYBIND11_MODULE(pybindings, m){
        py::module constraints = m.def_submodule("constraints");
        init_constraints(constraints);
        py::module parsing = m.def_submodule("parsing");
        init_parsing(parsing);

        m.attr("redirect_output") =
            py::capsule(
                new py::scoped_ostream_redirect(),
                [](void *sor) {
                    // std::cout << "add run " << addTimer.count() << "s" << std::endl;
                    delete static_cast<py::scoped_ostream_redirect *>(sor);
                });
        py::add_ostream_redirect(m, "ostream_redirect");
    }
#endif