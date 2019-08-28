/*
<%
cfg['compiler_args'] = ['-std=c++14', '-g', '-DPY_BINDINGS', '-O0']
setup_pybind11(cfg)
%>
*/

#undef NDEBUG

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
#include <algorithm>

#include <cstdlib>

#undef assert
#ifdef NDEBUG
#define assert(x) ((void)sizeof(x))
#else
#define assert(x) ((void)(!(x) && assert_handler(#x, __FILE__, __LINE__)))
#endif

class assertion_error: public std::runtime_error {
public:
    assertion_error(const std::string& what_arg):
        std::runtime_error(what_arg){}
};

bool assert_handler(std::string condition, std::string file, int line) {
    std::stringstream s;
    s << std::endl << file << ":" << line << " Assertion failed: '" << condition << "'";
    throw assertion_error(s.str());
    return false;
}

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
bool orderByCoeff(std::pair<T, int> &a, std::pair<T, int> &b) {
    return a.first < b.first;
}

template<typename T>
using FatInequalityPtr = std::unique_ptr<FatInequality<T>>;



enum class State {
    False = 2, True = 3, Unassigned = 0
};

State operator!(State s) {
    uint i = static_cast<uint>(s);
    i = (i >> 1) ^ i;
    // in case of unassigned (0b00) i >> 1 = 0b00 so nothing is flipped
    // in case of false (0b10) or true (0b11) i >> 1 = 0b01 so the right
    // most bit is flipped which changes true to false and vice versa
    return static_cast<State>(i);
}

class Assignment {
private:
    std::vector<State> v;

public:
    State* value;

    Assignment(int nVars)
        : v(2 * nVars + 1)
        , value(v.data() + nVars)
    {}

    void assign(int lit) {
        value[lit] = State::True;
        value[-lit] = State::False;
    }

    void unassign(int lit) {
        value[lit] = State::Unassigned;
        value[-lit] = State::Unassigned;
    }
};

template<typename T>
class Inequality;

struct PropState {
    size_t qhead = 0;
    bool conflict = false;
};

template<typename T>
class PropEngine {
private:
    size_t nVars;
    std::vector<std::vector<Inequality<T>*>> wl;
    std::vector<int> trail;

    PropState current;
    PropState base;

public:
    std::vector<Inequality<T>*>* watchlist;
    Assignment assignment;

    PropEngine(size_t _nVars)
        : nVars(_nVars)
        , wl(2 * _nVars + 1)
        , watchlist(wl.data() + _nVars)
        , assignment(_nVars)
    {

    }

    void propagate(int lit) {
        assignment.assign(lit);
        trail.push_back(lit);
    }

    void propagate(bool permanent = false) {
        while (current.qhead < trail.size() and !current.conflict) {
            int falsifiedLit = -trail[current.qhead];
            std::vector<Inequality<T>*>& ws = watchlist[falsifiedLit];
            std::vector<Inequality<T>*> wsTmp;
            wsTmp.reserve(ws.size());
            std::swap(ws, wsTmp);

            for (auto ineq:wsTmp) {
                if (ineq != nullptr) {
                    ineq->updateWatch(*this, falsifiedLit);
                }
            }
            current.qhead += 1;
        }
        if (permanent) {
            base = current;
        }
    }

    void _attach(Inequality<T>* ineq, bool permanent = true) {
        ineq->freeze(this->nVars);
        ineq->updateWatch(*this);
        propagate(permanent);
    }

    void attach(Inequality<T>* ineq) {
        _attach(ineq, true);
    }

    void detach(Inequality<T>* ineq) {
        if (ineq != nullptr) {
            ineq->clearWatches(*this);
        }
    }

    bool attachTmp(Inequality<T>* ineq) {
        _attach(ineq, false);
        bool result = isConflicting();
        reset();
        ineq->clearWatches(*this);
        return result;
    }

    void reset() {
        while (trail.size() > base.qhead) {
            assignment.unassign(trail.back());
            trail.pop_back();
        }
        current = base;
    }

    bool isConflicting(){
        return current.conflict;
    }

    void conflict() {
        current.conflict = true;
    }

    void watch(int lit, Inequality<T>* ineq) {
        watchlist[lit].push_back(ineq);
    }

};

/**
 * stores constraint in (literal) normalized form
 */
template<typename T>
class Inequality {
private:
    std::vector<T> coeffs;
    std::vector<int> lits;
    int degree;
    bool loaded = false;
    bool frozen = false;
    FatInequalityPtr<T> expanded;

    size_t watchSize = 0;

    static std::vector<FatInequalityPtr<T>> pool;

    void computeWatchSize() {
        if (coeffs.size() == 0) {
            return;
        }

        // improve: that is just lazy... we probably want to have
        // pair<T, int> in general.
        auto data = this->toPairs();
        sort(data.begin(), data.end(), orderByCoeff<T>);

        size_t i = 0;
        for (auto pair: data) {
            this->coeffs[i] = pair.first;
            this->lits[i]  = pair.second;
            i++;
        }

        // we propagate lit[i] if slack < coeff[i] so we
        // need to make sure that if no watch is falsified we
        // have always slack() >= maxCoeff
        T maxCoeff = coeffs.back();
        T value = -this->degree;

        i = 0;
        for (; i < coeffs.size(); i++) {
            assert(coeffs[i] <= maxCoeff);
            value += coeffs[i];
            if (value >= maxCoeff) {
                i++;
                break;
            }
        }

        watchSize = i;
    }

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

    void freeze(size_t numVars) {
        contract();
        for (int lit: this->lits) {
            assert(std::abs(lit) <= numVars);
        }
        frozen = true;
    }

    void clearWatches(PropEngine<T>& prop) {
        for (int lit: this->lits) {
            for (auto& ineq: prop.watchlist[lit]) {
                if (ineq == this) {
                    ineq = nullptr;
                }
            }
        }
    }

    void updateWatch(PropEngine<T>& prop, int falsifiedLit = 0) {
        if (this->lits.size() != this->coeffs.size()) abort();
        assert(this->lits.size() == this->coeffs.size());
        bool init = false;
        if (watchSize == 0) {
            init = true;
            computeWatchSize();
        }

        T slack = -this->degree;

        size_t j = this->watchSize;

        auto& value = prop.assignment.value;

        for (size_t i = 0; i < this->watchSize; i++) {
            if (value[this->lits[i]] == State::False) {
                for (;j < this->lits.size(); j++) {
                    if (value[this->lits[j]] != State::False) {
                        using namespace std;
                        swap(this->lits[i], this->lits[j]);
                        swap(this->coeffs[i], this->coeffs[j]);
                        prop.watch(this->lits[i], this);
                        j++;
                        break;
                    }
                }
            }

            if (value[this->lits[i]] != State::False) {
                slack += this->coeffs[i];
            }
            if (init || this->lits[i] == falsifiedLit) {
                // we could not swap out watcher, renew watch
                prop.watch(this->lits[i], this);
            }
        }

        if (slack < 0) {
            prop.conflict();
        } else {
            for (size_t i = 0; i < this->watchSize; i++) {
                if (this->coeffs[i] > slack
                    && value[this->lits[i]] == State::Unassigned)
                {
                    prop.propagate(this->lits[i]);
                }
            }
        }
    }

    Inequality* saturate(){
        assert(!frozen);
        Timer t(addTimer);
        contract();
        for (T& coeff: coeffs) {
            using namespace std;
            coeff = min(coeff, this->degree);
        }
        return this;
    }

    Inequality* divide(T divisor){
        assert(!frozen);
        Timer t(addTimer);
        contract();
        this->degree = divideAndRoundUp(this->degree, divisor);
        for (T& coeff: coeffs) {
            coeff = divideAndRoundUp(coeff, divisor);
        }
        return this;
    }

    Inequality* multiply(T factor){
        assert(!frozen);
        Timer t(addTimer);
        contract();
        this->degree *= factor;
        for (T& coeff: coeffs) {
            coeff *= factor;
        }
        return this;
    }

    Inequality* add(Inequality* other){
        assert(!frozen);
        Timer t(addTimer);
        expand();
        expanded->add(other->coeffs, other->lits, other->degree);
        return this;
    }

    void expand() {
        assert(!frozen);
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
            s << term.first << " ";
            if (term.second < 0) {
                s << "~";
            }
            s << "x" << abs(term.second) << " ";
        }
        s << ">= " << this->degree;
        return s.str();
    }

    bool implies(Inequality* other) {
        this->contract();
        other->contract();
        std::unordered_map<int, std::pair<T, int>> lookup;
        auto otherPairs = other->toPairs();
        for (auto term:otherPairs) {
            using namespace std;
            lookup.insert(make_pair(abs(term.second), term));
        }

        T weakenCost = 0;
        for (auto mine:this->toPairs()) {
            using namespace std;
            int var = abs(mine.second);

            auto search = lookup.find(var);
            if (search == lookup.end()) {
                weakenCost += mine.first;
            } else {
                auto theirs = search->second;
                if (mine.second != theirs.second) {
                    weakenCost += mine.first;
                } else if (mine.first > theirs.first) {
                    weakenCost += mine.first - theirs.first;
                }
            }
        }

        return this->degree - weakenCost >= other->degree;
    }

    Inequality* negated() {
        assert(!frozen);
        this->contract();
        this->degree = -this->degree + 1;
        for (size_t i = 0; i < this->coeffs.size(); i++) {
            this->degree += this->coeffs[i];
            this->lits[i] *= -1;
        }

        assert(this->coeffs.size() == this->lits.size());
        return this;
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
    Inequality<int> foo({1,1,1},{1,1,1},1);
    Inequality<int> baa({1,1,1},{-1,-1,-1},3);
    PropEngine<int> p(10);
    p.attach(&foo);
    p.attachTmp(&baa);

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

        py::class_<PropEngine<int>>(m, "PropEngine")
            .def(py::init<int>())
            .def("attach", &PropEngine<int>::attach)
            .def("detach", &PropEngine<int>::detach)
            .def("attachTmp", &PropEngine<int>::attachTmp)
            .def("reset", &PropEngine<int>::reset);


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
            .def("negated", &Inequality<int>::negated)
            .def("__eq__", &Inequality<int>::eq)
            .def("__repr__", &Inequality<int>::repr)
            .def("toOPB", &Inequality<int>::repr)
            .def("isContradiction", &Inequality<int>::isContradiction);

        py::add_ostream_redirect(m, "ostream_redirect");
    }
#endif