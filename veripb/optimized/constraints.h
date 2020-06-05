#include <vector>
#include <iostream>
#include <sstream>
#include <unordered_map>
#include <memory>
#include <chrono>
#include <algorithm>
#include <functional>

#include <cstdlib>
#include <iomanip>

using CoefType = int;
// use the following line istead to increas precision.
// using CoefType = long long;

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

inline bool assert_handler(std::string condition, std::string file, int line) {
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

template<typename T>
T divideAndRoundUp(T value, T divisor) {
    return (value + divisor - 1) / divisor;
}

typedef uint32_t LitData;

class Var {
public:
    LitData value;

    explicit Var(LitData value_)
        : value(value_)
    {}

    operator size_t() const {
        return value;
    }
};

class Lit {
private:
    LitData value;

    Lit(){}

public:
    explicit Lit(int t)
        : value((std::abs(t) << 1) + (t < 0 ? 1 : 0))
    {}

    Lit(Var var, bool isNegated)
        : value((var.value << 1) + static_cast<LitData>(isNegated))
    {}

    Var var() const {
        return Var(value >> 1);
    }

    bool isNegated() const {
        return value & 1;
    }

    Lit operator~() const {
        Lit res;
        res.value = value ^ 1;
        return res;
    }

    bool operator==(const Lit& other) const {
        return value == other.value;
    }

    bool operator!=(const Lit& other) const {
        return value != other.value;
    }

    bool operator>(const Lit& other) const {
        return value > other.value;
    }

    explicit operator size_t() const {
        return value;
    }

    explicit operator int() const {
        int result = var();
        assert(result != 0);
        if (isNegated()) {result = -result;}
        return result;
    }

    static Lit Undef() {
        return Lit(0);
    };
};

/*
 * some syntactic suggar to prevent us from accidentally using Var to
 * go in to a Lit inexed vector and vice versa.
 */
template<typename T>
class LitIndexedVec:public std::vector<T> {
public:
    using std::vector<T>::vector;
    // uncomment to allow indexing with integers
    // using std::vector<T>::operator[];
    T& operator[](Var idx) = delete;
    T& operator[](Lit idx) {
        return this->std::vector<T>::operator[](
            static_cast<size_t>(idx)
        );
    }
};

/*
 * some syntactic suggar to prevent us from accidentally using Var to
 * go in to a Lit inexed vector and vice versa.
 */
template<typename T>
class VarIndexedVec:public std::vector<T> {
public:
    using std::vector<T>::vector;
    // uncomment to allow indexing with integers
    // using std::vector<T>::operator[];
    T& operator[](Lit idx) = delete;
    T& operator[](Var idx) {
        return this->std::vector<T>::operator[](
            static_cast<size_t>(idx)
        );
    }
};

template<typename T>
T cpsign(T a, Lit b) {
    using namespace std;
    if (b.isNegated()) {
        return -abs(a);
    } else {
        return abs(a);
    }
}

template<typename T>
struct Term {
    T coeff;
    Lit lit;

    Term(T coeff_, Lit lit_)
        : coeff(coeff_)
        , lit(lit_)
        {}

    bool operator==(const Term<T>& other) const {
        return (this->coeff == other.coeff) && (this->lit == other.lit);
    }

    static
    std::vector<Term<T>> toTerms(
        std::vector<T>& coeffs,
        std::vector<int>& lits)
    {
        assert(coeffs.size() == lits.size());
        size_t size = coeffs.size();
        std::vector<Term<T>> result;
        result.reserve(size);
        for (size_t i = 0; i < size; i++) {
            result.emplace_back(coeffs[i], Lit(lits[i]));
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

inline State operator~(State s) {
    uint8_t i = static_cast<uint8_t>(s);
    i = (i >> 1) ^ i;
    // in case of unassigned (0b00) i >> 1 = 0b00 so nothing is flipped
    // in case of false (0b10) or true (0b11) i >> 1 = 0b01 so the right
    // most bit is flipped which changes true to false and vice versa
    return static_cast<State>(i);
}

class Assignment {
public:
    LitIndexedVec<State> value;

    Assignment(int nVars)
        : value(2 * (nVars + 1))
    {}

    void assign(Lit lit) {
        value[lit] = State::True;
        value[~lit] = State::False;
    }

    void unassign(Lit lit) {
        value[lit] = State::Unassigned;
        value[~lit] = State::Unassigned;
    }
};

template<typename T>
class Inequality;

template<typename T>
class FixedSizeInequality;

template<typename T>
class FixedSizeInequalityHandler;

template<typename T>
class PropEngine;

template<typename T>
struct WatchInfo {
    FixedSizeInequality<T>* ineq;
};

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
        for (size_t i = 0; i < this->watchSize; i++) {
            auto& ws = prop.watchlist[terms[i].lit];
            ws.erase(
                std::remove_if(
                    ws.begin(),
                    ws.end(),
                    [this](auto& watch){
                        return watch.ineq == this;
                    }
                ),
                ws.end()
            );
        }
    }

    template<bool init>
    void updateWatch(PropEngine<T>& prop, Lit falsifiedLit = Lit::Undef()) {
        if (init) {
            computeWatchSize();
        }

        T slack = -this->degree;

        size_t j = this->watchSize;

        auto& value = prop.assignment.value;

        for (size_t i = 0; i < this->watchSize; i++) {
            if (value[terms[i].lit] == State::False) {
                Lit old = terms[i].lit;
                for (;j < size(); j++) {
                    if (value[terms[j].lit] != State::False) {
                        using namespace std;
                        swap(terms[i], terms[j]);
                        WatchInfo<T> w;
                        w.ineq = this;
                        prop.watch(terms[i].lit, w);
                        j++;
                        break;
                    }
                }

                if (old != terms[i].lit) {
                    for (auto& w: prop.watchlist[old]) {
                        if (w.ineq == this) {
                            w.ineq = nullptr;
                        }
                    }
                }
            }

            if (value[terms[i].lit] != State::False) {
                slack += terms[i].coeff;
            }
            if (init || terms[i].lit == falsifiedLit) {
                // we could not swap out watcher, renew watch
                WatchInfo<T> w;
                w.ineq = this;
                prop.watch(terms[i].lit, w);
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

    size_t mem() const {
        return sizeof(FixedSizeInequality<T>) + size() * sizeof(Term<T>);
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

    FixedSizeInequalityHandler(FixedSizeInequalityHandler&& other) {
        ineq = other.ineq;
        other.ineq = nullptr;
    }

    FixedSizeInequalityHandler& operator=(FixedSizeInequalityHandler&& other) {
        this->free();
        ineq = other.ineq;
        other.ineq = nullptr;
    }

    FixedSizeInequalityHandler(std::vector<Term<T>>& terms, T degree) {
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

template<typename T>
class ConstraintDatabase {
private:
    typedef FixedSizeInequalityHandler<T> Handler;
    typedef std::vector<Handler> HandleList;
    HandleList handled;

public:
    void take(FixedSizeInequalityHandler<T>&& t) {
        handled.emplace_back(t);
    }

    void clean() {
        handled.erase(
            std::remove_if(
                handled.begin(),
                handled.end(),
                [](Handler& handler){
                    return handler->ineq.isDeleted();
                }
            ),
            handled.end()
        );
    }
};

template<typename T>
class PropEngine {
private:
    typedef WatchInfo<T> WatchedType;
    typedef std::vector<WatchedType> WatchList;
    typedef std::vector<FixedSizeInequality<T>*> OccursList;

    size_t nVars;
    std::vector<Lit> trail;

    PropState current;
    PropState base;

    size_t dbMem = 0;
    size_t cumDbMem = 0;
    size_t maxDbMem = 0;

public:
    Assignment assignment;
    LitIndexedVec<WatchList> watchlist;
    LitIndexedVec<OccursList> occurs;
    LitIndexedVec<Inequality<T>*> unattached;


    PropEngine(size_t _nVars)
        : nVars(_nVars)
        , assignment(_nVars)
        , watchlist(2 * (_nVars + 1))
        , occurs(2 * (_nVars + 1))
    {

    }

    void printStats() {
        std::cerr << "used database memory: "
            << std::fixed << std::setprecision(3)
            << static_cast<float>(dbMem) / 1024 / 1024 / 1024 << " GB" << std::endl;

        std::cerr << "cumulative database memory: "
            << std::fixed << std::setprecision(3)
            << static_cast<float>(cumDbMem) / 1024 / 1024 / 1024 << " GB" << std::endl;

        std::cerr << "maximal used database memory: "
            << std::fixed << std::setprecision(3)
            << static_cast<float>(maxDbMem) / 1024 / 1024 / 1024 << " GB" << std::endl;
    }

    void propagate(Lit lit) {
        assignment.assign(lit);
        trail.push_back(lit);
    }

    void propagate(bool permanent = false) {
        while (current.qhead < trail.size() and !current.conflict) {
            Lit falsifiedLit = ~trail[current.qhead];
            WatchList& ws = watchlist[falsifiedLit];
            WatchList wsTmp;
            wsTmp.reserve(ws.size());
            std::swap(ws, wsTmp);

            const uint lookAhead = 6;
            WatchedType* end = wsTmp.data() + wsTmp.size();
            for (WatchedType* next = wsTmp.data(); next != end; next++) {
                auto fetch = next + lookAhead;
                if (fetch < end) {
                    __builtin_prefetch(fetch->ineq);
                }
                if (next->ineq != nullptr) {
                    next->ineq->template updateWatch<false>(*this, falsifiedLit);
                }
            }

            current.qhead += 1;
        }

        while (current.qhead < trail.size()) {
            Lit falsifiedLit = ~trail[current.qhead];
            WatchList& ws = watchlist[falsifiedLit];

            ws.erase(
                std::remove_if(
                    ws.begin(),
                    ws.end(),
                    [](WatchedType& watch){
                        return watch.ineq == nullptr;
                    }
                ),
                ws.end()
            );

            current.qhead += 1;
        }

        if (permanent) {
            base = current;
        }
    }

    bool checkSat(std::vector<int>& lits) {
        attachUnattached();
        for (int lit: lits) {
            Lit l(lit);
            auto val = assignment.value[l];

            if (val == State::Unassigned) {
                propagate(l);
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
                auto val = assignment.value[Lit(var)];
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
        dbMem += ineq->mem();
        cumDbMem += ineq->mem();
        maxDbMem = std::max(dbMem, maxDbMem);
        unattached.push_back(ineq);
    }

    void attachUnattached() {
        for (Inequality<T>* ineq: unattached) {
            if (ineq != nullptr) {
                _attach(ineq, true);
            }
        }
        unattached.clear();
    }

    void detach(Inequality<T>* ineq) {
        if (ineq != nullptr) {
            dbMem -= ineq->mem();
            auto foundIt = std::find(
                unattached.rbegin(), unattached.rend(), ineq);

            if (foundIt != unattached.rend()) {
                std::swap(*foundIt, unattached.back());
                assert(unattached.back() == ineq);
                unattached.pop_back();
            } else {
                ineq->clearWatches(*this);
            }
        }
    }

    bool attachTmp(Inequality<T>* ineq) {
        attachUnattached();
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

    void watch(Lit lit, WatchInfo<T>& w) {
        watchlist[lit].push_back(w);
    }

    void addOccurence(Lit lit, FixedSizeInequality<T>& w) {
        occurs[lit].push_back(w);
    }

};

/**
 * stores constraint in (literal) normalized form, the sign of the
 * literal is stored in the coefficient
 */
template<typename T>
class FatInequality {
private:
    VarIndexedVec<T> coeffs;
    VarIndexedVec<uint8_t> used;
    std::vector<Var> usedList;
    T degree;
    bool bussy;

    void setCoeff(Var var, T value) {
        if (value != 0) {
            use(var);
            coeffs[var] = value;
        }
    }

    void use(Var var) {
        if (this->coeffs.size() <= var) {
            this->coeffs.resize(var.value + 1, 0);
            this->used.resize(var.value + 1, false);
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
        assert(!bussy && "critical error: I am used twice!");
        bussy = true;

        this->degree = ineq.degree;
        for (Term<T>& term: ineq) {
            T coeff = cpsign(term.coeff, term.lit);
            using namespace std;

            setCoeff(term.lit.var(), coeff);
        }
    }

    void unload(FixedSizeInequality<T>& ineq) {
        bussy = false;

        Term<T>* pos = ineq.begin();
        for (Var var: this->usedList) {
            T coeff = this->coeffs[var];
            Lit lit(var, coeff < 0);
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
        for (Var var: this->usedList) {
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
            Var var = term.lit.var();
            this->use(var);
            T a = this->coeffs[var];
            this->coeffs[var] = a + b;
            T cancellation = max<T>(0, max(abs(a), abs(b)) - abs(this->coeffs[var]));
            this->degree -= cancellation;
        }

        this->degree += other.degree;
    }
};

template<typename T>
using FatInequalityPtr = std::unique_ptr<FatInequality<T>>;

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

    Inequality(std::vector<Term<T>>& terms_, T degree_)
        : handler(terms_, degree_)
    {}

    Inequality(std::vector<Term<T>>&& terms_, T degree_)
        : handler(terms_, degree_)
    {}

    Inequality(
            std::vector<T>& coeffs,
            std::vector<int>& lits,
            T degree_)
        : Inequality(Term<T>::toTerms(coeffs, lits), degree_)
    {}

    Inequality(
            std::vector<T>&& coeffs,
            std::vector<int>&& lits,
            T degree_)
        : Inequality(Term<T>::toTerms(coeffs, lits), degree_)
    {}



    ~Inequality(){
        contract();
    }

    void freeze(size_t numVars) {
        contract();

        for (Term<T> term: *ineq) {
            assert(term.lit.var() <= numVars);
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

        if (ineq->degree <= 0 && ineq->size() > 0) {
            // nasty hack to shrink the constraint as this should not
            // happen frequently anyway.
            expand();
            contract();
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

    std::string toString(std::function<std::string(int)> varName) {
        contract();
        std::stringstream s;
        for (Term<T> &term: *ineq) {
            using namespace std;
            s << term.coeff << " ";
            if (term.lit.isNegated()) {
                s << "~";
            }
            s << varName(term.lit.var()) << " ";
        }
        s << ">= " << ineq->degree;
        return s.str();
    }

    std::string repr(){
        return toString([](int i){
            std::stringstream s;
            s << "x" << i;
            return s.str();
        });
    }

    bool implies(Inequality* other) {
        this->contract();
        other->contract();
        std::unordered_map<size_t, Term<T>> lookup;
        for (Term<T>& term:*other->ineq) {
            lookup.insert(std::make_pair(term.lit.var(), term));
        }

        T weakenCost = 0;
        for (Term<T>& mine:*ineq) {
            using namespace std;
            size_t var = mine.lit.var();

            auto search = lookup.find(var);
            if (search == lookup.end()) {
                weakenCost += mine.coeff;
            } else {
                auto theirs = search->second;
                if (mine.lit != theirs.lit) {
                    weakenCost += mine.coeff;
                } else if (mine.coeff > theirs.coeff) {
                    if (theirs.coeff < other->ineq->degree) {
                        // only weaken if target coefficient is not saturated
                        weakenCost += mine.coeff - theirs.coeff;
                    }
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
            term.lit = ~term.lit;
        }

        return this;
    }

    Inequality* copy(){
        contract();
        return new Inequality(*this);
    }

    bool isContradiction(){
        contract();
        T slack = -ineq->degree;
        for (Term<T> term: *ineq) {
            slack += term.coeff;
        }
        return slack < 0;
    }

    size_t mem() {
        contract();
        return ineq->mem();
    }
};

// we need to initialzie the static template member manually;
template<typename T>
std::vector<FatInequalityPtr<T>> Inequality<T>::pool;