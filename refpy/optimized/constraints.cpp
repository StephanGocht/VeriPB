/*
<%
cfg['compiler_args'] = ['-std=c++14', '-g', '-DPY_BINDINGS']
setup_pybind11(cfg)
%>
*/

#ifdef PY_BINDINGS
    #include <pybind11/pybind11.h>
    #include <pybind11/stl.h>
    #include <pybind11/iostream.h>

    namespace py = pybind11;
#endif


#include <vector>
#include <iostream>
#include <sstream>
#include <unordered_map>
#include <memory>
#include <chrono>

class Timer {
private:
    std::chrono::high_resolution_clock::time_point start;
    std::chrono::duration<double> &targetTimer;
public:
    Timer(std::chrono::duration<double> &targetTimer_):
        targetTimer(targetTimer_)
    {
        start = std::chrono::high_resolution_clock::now();
    }

    ~Timer(){
        auto stop = std::chrono::high_resolution_clock::now();
        targetTimer += (stop - start);
    }
};

std::chrono::duration<double> addTimer;

bool nonzero(std::vector<int> aList){
    for (auto &item: aList)
        if (item == 0)
            return false;
    return true;
}


template<typename T>
T divideAndRoundUp(T value, T divisor) {
    return (value + divisor - 1) / divisor;
}

template<typename T>
T cpsign(T a, T b) {
    using namespace std;
    if (b >= 0) {
        return abs(a);
    } else {
        return -abs(a);
    }
}


/**
 * stores constraint in (literal) normalized form, the sign of the
 * literal is stored in the coefficient
 */
template<typename T>
class FatInequality {
private:
    std::vector<T> coeffs;
    std::vector<uint8_t> used;
    std::vector<int> usedList;
    T degree;
    bool bussy;

    void setCoeff(uint var, T value) {
        if (value != 0) {
            use(var);
            coeffs[var] = value;
        }
    }

    void use(uint var) {
        if (this->coeffs.size() <= var) {
            this->coeffs.resize(var + 1, 0);
            this->used.resize(var + 1, false);
        }

        if (!used[var]) {
            used[var] = true;
            usedList.push_back(var);
        }
    }

public:
    FatInequality(){
        degree = 0;
        bussy = false;
    }

    void load(std::vector<T>& coeffs, std::vector<int>& lits, T degree) {
        if (bussy) {
            std::cout << "critical error: I am too bussy for this!" << std::endl;
            exit(1);
        }
        bussy = true;

        this->degree = degree;
        for (size_t i = 0; i < coeffs.size(); i++) {
            T coeff = cpsign(coeffs[i], lits[i]);
            using namespace std;
            int var = abs(lits[i]);

            setCoeff(var, coeff);
        }
    }

    void unload(std::vector<T>& coeffs, std::vector<int>& lits, T& degree) {
        bussy = false;
        coeffs.clear();
        coeffs.reserve(this->usedList.size());
        lits.clear();
        lits.reserve(this->usedList.size());

        for (int var: this->usedList) {
            T coeff = this->coeffs[var];
            T lit = cpsign(var, coeff);
            using namespace std;
            coeff = abs(coeff);

            if (coeff > 0) {
                lits.emplace_back(lit);
                coeffs.emplace_back(coeff);
            }

            this->coeffs[var] = 0;
            this->used[var] = false;
        }
        this->usedList.clear();

        degree = this->degree;
        this->degree = 0;
    }


    void add(const std::vector<T>& coeffs, const std::vector<int>& lits, const T degree) {
        for (size_t i = 0; i < coeffs.size(); i++) {
            using namespace std;
            T b = cpsign(coeffs[i], lits[i]);
            int var = abs(lits[i]);
            this->use(var);
            T a = this->coeffs[var];
            this->coeffs[var] = a + b;
            T cancellation = max(0, max(abs(a), abs(b)) - abs(this->coeffs[var]));
            this->degree -= cancellation;
        }

        this->degree += degree;
    }
};

template<typename T>
bool orderByVar(std::pair<T, int> &a, std::pair<T, int> &b) {
    return a.second > b.second;
}

template<typename T>
using FatInequalityPtr = std::unique_ptr<FatInequality<T>>;

/**
 * stores constraint in (literal) normalized form
 */
template<typename T>
class Inequality {
private:
    std::vector<T> coeffs;
    std::vector<int> lits;
    int degree;
    bool loaded;
    FatInequalityPtr<T> expanded;

    static std::vector<FatInequalityPtr<T>> pool;

public:
    Inequality(std::vector<int>&& coeffs_, std::vector<int>&& lits_, int degree_):
        coeffs(coeffs_),
        lits(lits_),
        degree(degree_)
    {
        loaded = false;
    }

    ~Inequality(){
        contract();
    }

    Inequality* saturate(){
        Timer t(addTimer);
        contract();
        for (T& coeff: coeffs) {
            using namespace std;
            coeff = min(coeff, this->degree);
        }
        return this;
    }

    Inequality* divide(T divisor){
        Timer t(addTimer);
        contract();
        this->degree = divideAndRoundUp(this->degree, divisor);
        for (T& coeff: coeffs) {
            coeff = divideAndRoundUp(coeff, divisor);
        }
        return this;
    }

    Inequality* multiply(T factor){
        Timer t(addTimer);
        contract();
        this->degree *= factor;
        for (T& coeff: coeffs) {
            coeff *= factor;
        }
        return this;
    }

    Inequality* add(Inequality* other){
        Timer t(addTimer);
        expand();
        expanded->add(other->coeffs, other->lits, other->degree);
        return this;
    }

    void expand() {
        if (!loaded) {
            loaded = true;
            if (pool.size() > 0) {
                expanded = std::move(pool.back());
                pool.pop_back();
            } else {
                expanded = std::make_unique<FatInequality<T>>();
            }
            expanded->load(coeffs, lits, degree);
        }
    }

    void contract() {
        Timer t(addTimer);
        if (loaded) {
            expanded->unload(coeffs, lits, degree);
            pool.push_back(std::move(expanded));
            loaded = false;
        }
    }

    std::vector<std::pair<T,int>> toPairs() {
        contract();
        std::vector<std::pair<T,int>> result;
        result.reserve(this->coeffs.size());
        for (size_t i = 0; i < coeffs.size(); i++) {
            result.emplace_back(coeffs[i], lits[i]);
        }

        return result;
    }

    bool eq(Inequality* other) {
        contract();
        auto mine = this->toPairs();
        sort(mine.begin(), mine.end(), orderByVar<T>);
        auto theirs = other->toPairs();
        sort(theirs.begin(), theirs.end(), orderByVar<T>);

        return (mine == theirs) && (this->degree == other->degree);
    }

    std::string repr() {
        contract();
        std::stringstream s;
        for (auto term: this->toPairs()) {
            using namespace std;
            s << term.first;
            if (term.second < 0) {
                s << "~";
            }
            s << "x" << abs(term.second) << " ";
        }
        s << ">= " << this->degree;
        return s.str();
    }

    bool implies(Inequality* other) {
        contract();
        std::unordered_map<int, std::pair<T, int>> lookup;
        auto otherPairs = other->toPairs();
        for (auto term:otherPairs) {
            using namespace std;
            lookup.insert(make_pair(abs(term.second), term));
        }

        for (auto mine:this->toPairs()) {
            using namespace std;
            int var = abs(mine.second);

            auto search = lookup.find(var);
            if (search == lookup.end()) {
                return false;
            } else {
                auto theirs = search->second;
                if (mine.second != theirs.second) {
                    return false;
                }
                if (mine.first > theirs.first) {
                    return false;
                }
            }
        }

        return true;
    }

    Inequality* copy(){
        Timer t(addTimer);
        contract();
        return new Inequality(
            std::vector<T>(coeffs),
            std::vector<int>(lits),
            degree);
    }

    bool isContradiction(){
        contract();
        int slack = -degree;
        for (int coeff: coeffs) {
            slack += coeff;
        }
        return slack < 0;
    }
};

// we need to initialzie the static template member manually;
template<typename T>
std::vector<FatInequalityPtr<T>> Inequality<T>::pool;


int main(int argc, char const *argv[])
{
    /* code */
    Inequality<int> foo({},{},1);
    std::cout << foo.isContradiction() << std::endl;
    return 0;
}


#ifdef PY_BINDINGS
    PYBIND11_MODULE(constraints, m){
        m.doc() = "Efficient implementation for linear combinations of constraints.";
        m.def("nonzero", &nonzero,
              "Test function",
              py::arg("aList"));
        m.attr("redirect_output") =
            py::capsule(
                new py::scoped_ostream_redirect(),
                [&addTimer](void *sor) {
                    // std::cout << "add run " << addTimer.count() << "s" << std::endl;
                    delete static_cast<py::scoped_ostream_redirect *>(sor);
                });

        py::class_<Inequality<int>>(m, "CppInequality")
            .def(py::init<std::vector<int>&&, std::vector<int>&&, int>())
            .def("saturate", &Inequality<int>::saturate)
            .def("divide", &Inequality<int>::divide)
            .def("multiply", &Inequality<int>::multiply)
            .def("add", &Inequality<int>::add)
            .def("contract", &Inequality<int>::contract)
            .def("copy", &Inequality<int>::copy)
            .def("implies", &Inequality<int>::implies)
            .def("expand", &Inequality<int>::expand)
            .def("contract", &Inequality<int>::contract)
            .def("__eq__", &Inequality<int>::eq)
            .def("__repr__", &Inequality<int>::repr)
            .def("isContradiction", &Inequality<int>::isContradiction);

        py::add_ostream_redirect(m, "ostream_redirect");
    }
#endif