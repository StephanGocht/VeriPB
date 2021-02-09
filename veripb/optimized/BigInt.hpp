#pragma once

#ifdef PY_BINDINGS
    #include <pybind11/pybind11.h>
    #include <pybind11/stl.h>
    #include <pybind11/iostream.h>
    #include <pybind11/functional.h>
    namespace py = pybind11;
#endif

// #include <boost/multiprecision/gmp.hpp>
// using BigInt = boost::multiprecision::mpz_int;
// namespace std {
//     inline BigInt abs(BigInt num) {
//         return boost::multiprecision::abs(num);
//     }
// }
// inline std::string toString(BigInt& num, int base = 10) {
//     num.str(base);
// }
#include <string_view>

#include <gmpxx.h>
using BigInt = mpz_class;
namespace std {
    inline BigInt abs(BigInt& num) {
        return ::abs(num);
    }

    template<>
    struct hash<mpz_class> {
        std::size_t operator()(const mpz_class& y) const {
            const mpz_srcptr x = y.get_mpz_t();

            string_view view { reinterpret_cast<char*>(x->_mp_d), abs(x->_mp_size)
                    * sizeof(mp_limb_t) };
            size_t result = hash<string_view> { }(view);

            // produce different hashes for negative x
            if (x->_mp_size < 0) {
                result ^= std::hash<size_t>()(0);
            }

            return result;
        }
    };
}

inline std::string toString(BigInt& num, int base = 10) {
    return num.get_str(base);
}

#ifdef PY_BINDINGS
    namespace pybind11 { namespace detail {
        template <> struct type_caster<BigInt> {
        public:
            /**
             * This macro establishes the name 'BigInt' in
             * function signatures and declares a local variable
             * 'value' of type BigInt
             */
            PYBIND11_TYPE_CASTER(BigInt, _("BigInt"));

            /**
             * Conversion part 1 (Python->C++): convert a PyObject into a BigInt
             * instance or return false upon failure. The second argument
             * indicates whether implicit conversions should be applied.
             */
            bool load(handle src, bool) {
                /* Extract PyObject from handle */
                PyObject *source = src.ptr();
                /* Try converting into a Python integer value */
                PyObject *pyNumber = nullptr;
                PyObject *pyString = nullptr;

                bool error = false;

                pyNumber = PyNumber_Long(source);
                if (!pyNumber) {
                    error = true;
                } else {
                    // using base 10 because boost multiprecision does not
                    // seem to support sign for base 16
                    pyString = PyNumber_ToBase(pyNumber,10);
                    if (!pyString) {
                        error = true;
                    } else {
                        const char* string = PyUnicode_AsUTF8(pyString);
                        if (string) {
                            try {
                                value = BigInt(string);
                            } catch (const std::runtime_error& e) {
                                error = true;
                            }
                        }


                    }

                }

                error |= (PyErr_Occurred() != nullptr);
                if (pyNumber)
                    Py_DECREF(pyNumber);
                if (pyString)
                    Py_DECREF(pyString);
                return !error;
            }

            /**
             * Conversion part 2 (C++ -> Python): convert an BigInt instance into
             * a Python object. The second and third arguments are used to
             * indicate the return value policy and parent object (for
             * ``return_value_policy::reference_internal``) and are generally
             * ignored by implicit casters.
             */
            static handle cast(BigInt src, return_value_policy /* policy */, handle /* parent */) {
                std::string string = toString(src,10);
                PyObject *pyString = PyUnicode_FromString(string.c_str());
                PyObject *result = PyNumber_Long(pyString);
                Py_DECREF(pyString);
                return result;
            }
        };
    }} // namespace pybind11::detail
#endif // ifdef PY_BINDINGS