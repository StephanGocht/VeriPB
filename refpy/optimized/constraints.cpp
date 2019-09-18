/*
<%
cfg['compiler_args'] = ['-std=c++14', '-g', '-DPY_BINDINGS']
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


template<typename T>
struct Term {
    T coeff;
    int lit;

    Term(T coeff_, int lit_)
        : coeff(coeff_)
        , lit(lit_)
        {}

    bool operator==(const Term<T>& other) const {
        return (this->coeff == other.coeff) && (this->lit == other.lit);
    }

    static
    std::vector<Term<T>> toTerms(
        std::vector<int>& coeffs,
        std::vector<int>& lits)
    {
        assert(coeffs.size() == lits.size());
        size_t size = coeffs.size();
        std::vector<Term<T>> result;
        result.reserve(size);
        for (size_t i = 0; i < size; i++) {
            result.emplace_back(coeffs[i], lits[i]);
        }
        return result;
    }
};

template<typename T>
bool orderByVar(T &a, T &b) {
    return a.lit > b.lit;
}

template<typename T>
bool orderByCoeff(T &a, T &b) {
    return a.coeff < b.coeff;
}

enum class State : uint8_t {
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

template<typename T>
class FixedSizeInequalityHandler;

template<typename T>
class PropEngine;

template<typename T>
class FixedSizeInequality {
private:
    int _size;
    size_t watchSize = 0;

public:
    T degree;
    T maxCoeff;
    Term<T> terms[];

private:
    friend FixedSizeInequalityHandler<T>;

    FixedSizeInequality(size_t size)
        :_size(size) {
    }

    FixedSizeInequality(FixedSizeInequality& other)
        : _size(other.size())
    {
        using namespace std;
        copy(other.begin(), other.end(), this->begin());
        this->degree = other.degree;
    }

    FixedSizeInequality(std::vector<Term<T>>& terms, T _degree)
        : _size(terms.size())
        , degree(_degree)
    {
        std::copy(terms.begin(),terms.end(), this->begin());
    }

    void computeWatchSize() {
        if (size() == 0) {
            return;
        }

        // improve: that is just lazy... we probably want to have
        // pair<T, int> in general.
        std::sort(begin(), end(), orderByCoeff<Term<T>>);

        // we propagate lit[i] if slack < coeff[i] so we
        // need to make sure that if no watch is falsified we
        // have always slack() >= maxCoeff
        maxCoeff = terms[size() - 1].coeff;
        T value = -this->degree;

        size_t i = 0;
        for (; i < size(); i++) {
            assert(terms[i].coeff <= maxCoeff);
            value += terms[i].coeff;
            if (value >= maxCoeff) {
                i++;
                break;
            }
        }

        watchSize = i;
    }


public:
    void clearWatches(PropEngine<T>& prop) {
        for (Term<T>& term: *this) {
            for (auto& ineq: prop.watchlist[term.lit]) {
                if (ineq == this) {
                    ineq = nullptr;
                }
            }
        }
    }

    template<bool init>
    void updateWatch(PropEngine<T>& prop, int falsifiedLit = 0) {
        if (init) {
            computeWatchSize();
        }

        T slack = -this->degree;

        size_t j = this->watchSize;

        auto& value = prop.assignment.value;

        for (size_t i = 0; i < this->watchSize; i++) {
            if (value[terms[i].lit] == State::False) {
                for (;j < size(); j++) {
                    if (value[terms[j].lit] != State::False) {
                        using namespace std;
                        swap(terms[i], terms[j]);
                        prop.watch(terms[i].lit, this);
                        j++;
                        break;
                    }
                }
            }

            if (value[terms[i].lit] != State::False) {
                slack += terms[i].coeff;
            }
            if (init || terms[i].lit == falsifiedLit) {
                // we could not swap out watcher, renew watch
                prop.watch(terms[i].lit, this);
            }
        }

        if (slack < 0) {
            prop.conflict();
        } else if (slack < maxCoeff) {
            for (size_t i = 0; i < this->watchSize; i++) {
                if (terms[i].coeff > slack
                    && value[terms[i].lit] == State::Unassigned)
                {
                    prop.propagate(terms[i].lit);
                }
            }
        }
    }

    size_t size() const {
        return _size;
    }

    const Term<T>* begin() const {
        return terms;
    }

    const Term<T>* end() const {
        return terms + size();
    }

    Term<T>* begin() {
        return terms;
    }

    Term<T>* end() {
        return terms + size();
    }
};

struct PropState {
    size_t qhead = 0;
    bool conflict = false;
};


// todo: can we type this with a template parameter?
typedef FixedSizeInequality<int> WatchedType;

typedef std::vector<WatchedType*> WatchList;


template<typename T>
class PropEngine {
private:
    size_t nVars;
    std::vector<WatchList> wl;
    std::vector<int> trail;

    PropState current;
    PropState base;

public:
    WatchList* watchlist;
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
            WatchList& ws = watchlist[falsifiedLit];
            WatchList wsTmp;
            wsTmp.reserve(ws.size());
            std::swap(ws, wsTmp);

            const uint lookAhead = 6;
            WatchedType** end = wsTmp.data() + wsTmp.size();
            for (WatchedType** next = wsTmp.data(); next != end; next++) {
                auto fetch = next + lookAhead;
                if (fetch < end) {
                    __builtin_prefetch(*fetch);
                }
                if (*next != nullptr) {
                    (*next)->template updateWatch<false>(*this, falsifiedLit);
                }
            }
            current.qhead += 1;
        }
        if (permanent) {
            base = current;
        }
    }

    bool checkSat(std::vector<int>& lits) {
        for (int lit: lits) {
            auto val = assignment.value[lit];

            if (val == State::Unassigned) {
                propagate(lit);
            } else if (val == State::False) {
                conflict();
                break;
            }
        }

        propagate();

        bool success = false;
        if (!current.conflict) {
            success = true;
            for (uint var = 1; var <= nVars; var++) {
                auto val = assignment.value[var];
                if (val == State::Unassigned) {
                    success = false;
                    break;
                }
            }
        }

        reset();
        return success;
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

    void watch(int lit, WatchedType* ineq) {
        watchlist[lit].push_back(ineq);
    }

};

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

    void load(FixedSizeInequality<T>& ineq) {
        if (bussy) {
            std::cout << "critical error: I am too bussy for this!" << std::endl;
            exit(1);
        }
        bussy = true;

        this->degree = ineq.degree;
        for (Term<T>& term: ineq) {
            T coeff = cpsign(term.coeff, term.lit);
            using namespace std;
            int var = abs(term.lit);

            setCoeff(var, coeff);
        }
    }

    void unload(FixedSizeInequality<T>& ineq) {
        bussy = false;

        Term<T>* pos = ineq.begin();
        for (int var: this->usedList) {
            T coeff = this->coeffs[var];
            T lit = cpsign(var, coeff);
            using namespace std;
            coeff = abs(coeff);

            if (coeff > 0) {
                assert(pos != ineq.end());
                pos->coeff = coeff;
                pos->lit = lit;
                pos += 1;
            }

            this->coeffs[var] = 0;
            this->used[var] = false;
        }
        assert(pos == ineq.end());
        this->usedList.clear();

        ineq.degree = this->degree;
        this->degree = 0;
    }

    size_t size() {
        size_t result = 0;
        for (int var: this->usedList) {
            if (this->coeffs[var] != 0) {
                result += 1;
            }
        }
        return result;
    }


    void add(const FixedSizeInequality<T>& other) {
        for (const Term<T> &term:other) {
            using namespace std;
            T b = cpsign(term.coeff, term.lit);
            int var = abs(term.lit);
            this->use(var);
            T a = this->coeffs[var];
            this->coeffs[var] = a + b;
            T cancellation = max(0, max(abs(a), abs(b)) - abs(this->coeffs[var]));
            this->degree -= cancellation;
        }

        this->degree += other.degree;
    }
};

template<typename T>
using FatInequalityPtr = std::unique_ptr<FatInequality<T>>;

template<typename T>
class FixedSizeInequalityHandler {
private:
    void* malloc(size_t size) {
        capacity = size;
        size_t memSize = sizeof(FixedSizeInequality<T>) + size * sizeof(Term<T>);
        // return std::aligned_alloc(64, memSize);
        return std::malloc(memSize);
    }

    void free(){
        if (ineq != nullptr) {
            ineq->~FixedSizeInequality<T>();
            std::free(ineq);
            ineq = nullptr;
        }
    }

public:
    FixedSizeInequality<T>* ineq = nullptr;
    size_t capacity = 0;

    FixedSizeInequalityHandler(FixedSizeInequalityHandler& other) {
        void* addr = malloc(other.ineq->size());
        ineq = new (addr) FixedSizeInequality<T>(*other.ineq);
    }

    FixedSizeInequalityHandler(std::vector<Term<T>>& terms, int degree) {
        void* addr = malloc(terms.size());
        ineq = new (addr) FixedSizeInequality<T>(terms, degree);
    }

    void resize(size_t size) {
        if (ineq != nullptr && size == capacity) {
            return;
        }
        free();
        void* addr = malloc(size);
        ineq = new (addr) FixedSizeInequality<T>(size);
    }

    ~FixedSizeInequalityHandler(){
        free();
    }
};

/**
 * stores constraint in (literal) normalized form
 *
 * The inequality can have three states: normal, expanded, frozen it
 * switches automatically between normal and expanded as needed. Once
 * the inequality is frozen it can not switch back and all operations
 * that would modify the inequality are disallowed.
 */
template<typename T>
class Inequality {
private:
    bool loaded = false;
    bool frozen = false;
    FatInequalityPtr<T> expanded;

    FixedSizeInequalityHandler<T> handler;
    FixedSizeInequality<T>* &ineq = handler.ineq;

    static std::vector<FatInequalityPtr<T>> pool;

public:
    Inequality(Inequality& other)
        : handler(other.handler) {
            assert(other.loaded == false);
    }

    Inequality(std::vector<Term<T>>&& terms_, int degree_)
        : handler(terms_, degree_)
    {}

    Inequality(
            std::vector<int>& coeffs,
            std::vector<int>& lits,
            int degree_)
        : Inequality(Term<T>::toTerms(coeffs, lits), degree_)
    {}

    Inequality(
            std::vector<int>&& coeffs,
            std::vector<int>&& lits,
            int degree_)
        : Inequality(Term<T>::toTerms(coeffs, lits), degree_)
    {}



    ~Inequality(){
        contract();
    }

    void freeze(size_t numVars) {
        contract();

        for (Term<T> term: *ineq) {
            assert(static_cast<uint>(std::abs(term.lit)) <= numVars);
        }

        // todo we are currently using twice the memory neccessary, we would want to
        // switch computation of eq and so on to the fixed size inequality as well.
        frozen = true;
    }

    void clearWatches(PropEngine<T>& prop) {
        assert(frozen);
        ineq->clearWatches(prop);
    }

    void updateWatch(PropEngine<T>& prop) {
        assert(frozen && "Call freeze() first.");
        // ineq->updateWatch<true>(prop);
        ineq->template updateWatch<true>(prop);
    }

    Inequality* saturate(){
        assert(!frozen);
        contract();
        for (Term<T>& term: *ineq) {
            using namespace std;
            term.coeff = min(term.coeff, ineq->degree);
        }
        return this;
    }

    Inequality* divide(T divisor){
        assert(!frozen);
        contract();
        ineq->degree = divideAndRoundUp(ineq->degree, divisor);
        for (Term<T>& term: *ineq) {
            term.coeff = divideAndRoundUp(term.coeff, divisor);
        }
        return this;
    }

    Inequality* multiply(T factor){
        assert(!frozen);
        contract();
        ineq->degree *= factor;
        for (Term<T>& term: *ineq) {
            term.coeff *= factor;
        }
        return this;
    }

    Inequality* add(Inequality* other){
        assert(!frozen);
        expand();
        other->contract();
        expanded->add(*other->ineq);
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
            expanded->load(*ineq);
        }
    }

    void contract() {
        if (loaded) {
            assert(!frozen);
            handler.resize(expanded->size());
            expanded->unload(*ineq);
            pool.push_back(std::move(expanded));
            loaded = false;
        }
    }

    bool eq(Inequality* other) {
        contract();
        other->contract();
        std::vector<Term<T>> mine(ineq->begin(), ineq->end());
        sort(mine.begin(), mine.end(), orderByVar<Term<T>>);
        std::vector<Term<T>> theirs(other->ineq->begin(), other->ineq->end());
        sort(theirs.begin(), theirs.end(), orderByVar<Term<T>>);
        return (mine == theirs) && (ineq->degree == other->ineq->degree);
    }

    std::string repr() {
        contract();
        std::stringstream s;
        for (Term<T> &term: *ineq) {
            using namespace std;
            s << term.coeff << " ";
            if (term.lit < 0) {
                s << "~";
            }
            s << "x" << abs(term.lit) << " ";
        }
        s << ">= " << ineq->degree;
        return s.str();
    }

    bool implies(Inequality* other) {
        this->contract();
        other->contract();
        std::unordered_map<int, Term<T>> lookup;
        for (Term<T>& term:*other->ineq) {
            using namespace std;
            lookup.insert(make_pair(abs(term.lit), term));
        }

        T weakenCost = 0;
        for (Term<T>& mine:*ineq) {
            using namespace std;
            int var = abs(mine.lit);

            auto search = lookup.find(var);
            if (search == lookup.end()) {
                weakenCost += mine.coeff;
            } else {
                auto theirs = search->second;
                if (mine.lit != theirs.lit) {
                    weakenCost += mine.coeff;
                } else if (mine.coeff > theirs.coeff) {
                    weakenCost += mine.coeff - theirs.coeff;
                }
            }
        }

        return ineq->degree - weakenCost >= other->ineq->degree;
    }

    Inequality* negated() {
        assert(!frozen);
        this->contract();
        ineq->degree = -ineq->degree + 1;
        for (Term<T>& term:*ineq) {
            ineq->degree += term.coeff;
            term.lit *= -1;
        }

        return this;
    }

    Inequality* copy(){
        contract();
        return new Inequality(*this);
    }

    bool isContradiction(){
        contract();
        int slack = -ineq->degree;
        for (Term<T> term: *ineq) {
            slack += term.coeff;
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
    {
    Inequality<int> foo({1,1,1},{1,1,1},1);
    Inequality<int> baa({1,1,1},{-1,-1,-1},3);
    PropEngine<int> p(10);
    foo.eq(&baa);
    foo.implies(&baa);
    foo.negated();
    Inequality<int> test(baa);
    p.attach(&foo);
    p.attachTmp(&baa);
    }
    Inequality<int> foo({1,1,1},{1,1,1},1);

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
                [](void *sor) {
                    // std::cout << "add run " << addTimer.count() << "s" << std::endl;
                    delete static_cast<py::scoped_ostream_redirect *>(sor);
                });

        py::class_<PropEngine<int>>(m, "PropEngine")
            .def(py::init<int>())
            .def("attach", &PropEngine<int>::attach)
            .def("detach", &PropEngine<int>::detach)
            .def("attachTmp", &PropEngine<int>::attachTmp)
            .def("reset", &PropEngine<int>::reset)
            .def("checkSat", &PropEngine<int>::checkSat);


        py::class_<Inequality<int>>(m, "CppInequality")
            .def(py::init<std::vector<int>&, std::vector<int>&, int>())
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