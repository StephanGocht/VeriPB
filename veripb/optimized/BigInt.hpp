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
#include <functional>


template <class T>
inline void hash_combine(std::size_t& seed, const T& v)
{
    std::hash<T> hasher;
    seed ^= hasher(v) + 0x9e3779b9 + (seed<<6) + (seed>>2);
}

#include <gmpxx.h>
using BigInt = mpz_class;

template<typename Out, typename In>
Out convertInt(In value) {
    return value;
}

template<>
inline int32_t convertInt(mpz_class value) {
    return value.get_si();
}

namespace std {
    inline BigInt abs(BigInt& num) {
        return ::abs(num);
    }

    template<>
    struct hash<mpz_class> {
        std::size_t operator()(const mpz_class& y) const {
            if (y.fits_slong_p()) {
                // this check is crucial to get the same result for
                // different int types, i.e., we want that the hash
                // value of BigInt(1) is the same as the hash value of
                // int32_t(1)
                static_assert(sizeof(long) == 8, "If long is not 64 "
                "bit then hashing bitween 64 bit integer and BigInt will "
                "not be identical.");
                return std::hash<long>()(y.get_si());
            }

            const mpz_srcptr x = y.get_mpz_t();

            size_t hash = 0;
            hash_combine(hash, x->_mp_size);

            size_t limb_size = abs(x->_mp_size);

            if (limb_size > 2) {
                hash_combine(hash, x->_mp_d[0]);
                hash_combine(hash, x->_mp_d[limb_size - 1]);
            } else {
                for (size_t i = 0; i < limb_size; i++) {
                    hash_combine(hash, x->_mp_d[i]);
                }
            }

            return hash;
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