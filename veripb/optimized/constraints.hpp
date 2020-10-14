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
#include <type_traits>

#include "BigInt.hpp"

using CoefType = BigInt;
// using CoefType = int;
// use the following line istead to increas precision.
// using CoefType = long long;

// #undef NDEBUG

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

public:
    Lit(){}

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

inline std::ostream& operator<<(std::ostream& os, const Var& v) {
    os << "x" << v.value;
    return os;
}

inline std::ostream& operator<<(std::ostream& os, const Lit& v) {
    if (v.isNegated()) {
        os << "~";
    }
    os << v.var();
    return os;
}

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
    const T& operator[](Lit idx) const {
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
    const T& operator[](Var idx) const {
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

    Term(){}

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
inline std::ostream& operator<<(std::ostream& os, const Term<T>& term) {
    os << term.coeff << " " << term.lit;
    return os;
}

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
        : value(2 * (nVars + 1), State::Unassigned)
    {}

    void assign(Lit lit) {
        value[lit] = State::True;
        value[~lit] = State::False;
    }

    void unassign(Lit lit) {
        value[lit] = State::Unassigned;
        value[~lit] = State::Unassigned;
    }

    void resize(size_t nVars) {
        value.resize(2 * (nVars + 1), State::Unassigned);
    }

    State& operator[](Lit lit) {
        return value[lit];
    }

    State operator[](Lit lit) const {
        return this->value[lit];
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
    Lit other;

    WatchInfo()
        : ineq(nullptr)
        , other(Lit::Undef()) {}
};


template<typename Element>
class InlineVec {
private:
    uint32_t _size;
    Element data[];

    // warning requires custom allocation
    InlineVec(size_t size)
        : _size(size) {};

    template<typename T>
    friend class FixedSizeInequality;
public:

    ~InlineVec(){
        this->erase(this->begin(), this->end());
    }

    void erase(Element* begin, Element* end) {
        size_t removed = end - begin;
        assert(removed >= 0);
        for (;begin != end; begin++) {
            begin->~Element();
        }
        _size = size() - removed;
    }

    size_t size() const {
        return _size;
    }

    Element& operator[](size_t index) {
        return data[index];
    }

    const Element* begin() const {
        return data;
    }

    const Element* end() const {
        return data + size();
    }

    Element* begin() {
        return data;
    }

    Element* end() {
        return data + size();
    }
};

template<typename TVec, typename TElement>
class extra_size_requirement {
public:
    static size_t value();
};

template<typename TElement>
class extra_size_requirement<std::vector<TElement>, TElement> {
public:
    static size_t value(size_t numElements) {
        return 0;
    };
};

template<typename TElement>
class extra_size_requirement<InlineVec<TElement>, TElement> {
public:
    static size_t value(size_t numElements) {
        return numElements * sizeof(TElement);
    };
};

template<typename T>
class FixedSizeInequality {
private:
    uint32_t watchSize = 0;

public:
    T degree;
    T maxCoeff;

    using TElement = Term<T>;
    using TVec = std::conditional_t<
            std::is_trivially_destructible<T>::value,
            InlineVec<TElement>,
            std::vector<TElement>
        >;
    // Term<T> terms[];
    TVec terms;

private:
    friend FixedSizeInequalityHandler<T>;

    FixedSizeInequality(size_t size)
        :terms(size) {
    }

    FixedSizeInequality(const FixedSizeInequality<T>& other)
        : terms(other.terms.size())
    {
        using namespace std;
        copy(other.terms.begin(), other.terms.end(), this->terms.begin());
        this->degree = other.degree;
    }

    FixedSizeInequality(std::vector<Term<T>>& _terms, T _degree)
        : degree(_degree)
        , terms(_terms.size())
    {
        std::copy(_terms.begin(),_terms.end(), this->terms.begin());
    }

    void computeWatchSize() {
        if (terms.size() == 0) {
            return;
        }

        // improve: that is just lazy... we probably want to have
        // pair<T, int> in general.
        std::sort(terms.begin(), terms.end(), orderByCoeff<Term<T>>);

        // we propagate lit[i] if slack < coeff[i] so we
        // need to make sure that if no watch is falsified we
        // have always slack() >= maxCoeff
        maxCoeff = terms[terms.size() - 1].coeff;
        T value = -this->degree;

        size_t i = 0;
        for (; i < terms.size(); i++) {
            assert(terms[i].coeff <= maxCoeff);
            value += terms[i].coeff;
            if (value >= maxCoeff) {
                i++;
                break;
            }
        }

        watchSize = i;
    }

    void registerOccurence(PropEngine<T>& prop) {
        for (Term<T>& term: this->terms) {
            prop.addOccurence(term.lit, *this);
        }
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


    void swapWatch(PropEngine<T>& prop, size_t& i, size_t& j, bool& keepWatch, Lit& blockingLiteral, Lit& falsifiedLit, bool init) {
        const Lit old = terms[i].lit;
        const Lit nw = terms[j].lit;
        if (old != falsifiedLit && !init) {
            auto& wl = prop.watchlist[old];
            int found = 0;
            for (size_t i = 0; i < wl.size(); ++i) {
                auto& w = wl[i];
                if (w.ineq == this) {
                    std::swap(w, wl.back());
                    wl.pop_back();
                    // break;
                    found += 1;
                }
            }
            assert(found == 1);
        } else {
            keepWatch = false;
        }

        std::swap(terms[i], terms[j]);

        WatchInfo<T> w;
        w.ineq = this;
        w.other = blockingLiteral;
        prop.watch(nw, w);
    }

    // returns if the watch is kept
    template<bool autoInit>
    bool updateWatch(PropEngine<T>& prop, Lit falsifiedLit = Lit::Undef()) {
        // bool isSat = false;
        bool keepWatch = true;
        prop.visit += 1;

        // std::cout << "updateWatch: " << *this << std::endl;
        const bool init = autoInit && (watchSize == 0);
        if (init) {
            computeWatchSize();
            registerOccurence(prop);
        }

        T slack = -this->degree;

        size_t j = this->watchSize;
        size_t k = this->watchSize;

        auto& value = prop.assignment.value;

        // if (this->watchSize == 2 && this->degree == 1) {
        //     if (!init && falsifiedLit != Lit::Undef() && terms[0].lit != falsifiedLit && terms[1].lit != falsifiedLit) {
        //         return;
        //     } else  {
        //         for (int i : {0,1}) {
        //             if (value[terms[i].lit] == State::True) {
        //                 WatchInfo<T> w;
        //                 w.ineq = this;
        //                 // w.other = terms[i].lit;
        //                 prop.watch(falsifiedLit, w);
        //                 return;
        //             }
        //         }
        //     }
        // }

        Lit blockingLiteral = Lit::Undef();
        if (terms.size() >= 2) {
            if (std::abs(terms[1].coeff) >= degree) {
                blockingLiteral = terms[1].lit;
            }
        }

        for (size_t i = 0; i < this->watchSize; i++) {
            if (value[terms[i].lit] == State::False) {
                if (init) {for (;k < terms.size(); k++) {
                    if (prop.phase[terms[k].lit] == State::True) {
                        swapWatch(prop,i,k,keepWatch,blockingLiteral,falsifiedLit,init);
                        slack += terms[i].coeff;
                        k++;
                        goto found_new_watch;
                    }
                }}
                for (;j < terms.size(); j++) {
                    if (value[terms[j].lit] != State::False) {
                        swapWatch(prop,i,j,keepWatch,blockingLiteral,falsifiedLit,init);
                        slack += terms[i].coeff;
                        j++;
                        goto found_new_watch;
                    }
                }

                found_new_watch:
                ;
            } else {
                slack += terms[i].coeff;

                if (init) {
                    WatchInfo<T> w;
                    w.ineq = this;
                    prop.watch(terms[i].lit, w);
                }
            }

            if (std::abs(terms[i].coeff) >= degree) {
                blockingLiteral = terms[i].lit;
            }
        }

        // for (size_t i = 0; i < size(); i++) {
        //     if (value[terms[i].lit] == State::True) {
        //         isSat = true;
        //         break;
        //     }
        // }

        // if (isSat) {
        //     prop.visit_sat += 1;
        // }

        if (slack < 0) {
            prop.conflict();
            prop.visit_required += 1;
        } else if (slack < maxCoeff) {
            for (size_t i = 0; i < this->watchSize; i++) {
                if (terms[i].coeff > slack
                    && value[terms[i].lit] == State::Unassigned)
                {
                    prop.enqueue(terms[i].lit);
                    prop.visit_required += 1;
                }
            }
        }

        return keepWatch;
    }

    void negate() {
        this->degree = -this->degree + 1;
        for (Term<T>& term:this->terms) {
            this->degree += term.coeff;
            term.lit = ~term.lit;
        }
    }

    bool implies(const FixedSizeInequality& other) const {
        std::unordered_map<size_t, Term<T>> lookup;
        for (const Term<T>& term:other.terms) {
            lookup.insert(std::make_pair(term.lit.var(), term));
        }

        T weakenCost = 0;
        for (const Term<T>& mine:this->terms) {
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
                    if (theirs.coeff < other.degree) {
                        // only weaken if target coefficient is not saturated
                        weakenCost += mine.coeff - theirs.coeff;
                    }
                }
            }
        }

        return this->degree - weakenCost >= other.degree;
    }

    bool isSatisfied(const Assignment& a) const {
        T slack = -this->degree;
        for (const Term<T>& term:this->terms) {
            if (a[term.lit] == State::True) {
                slack += term.coeff;
            }
        }
        return slack >= 0;
    }

    void restrictBy(const Assignment& a){
        this->terms.erase(
            std::remove_if(
                this->terms.begin(),
                this->terms.end(),
                [this, &a](Term<T>& term){
                    switch (a[term.lit]) {
                        case State::True:
                            this->degree -= term.coeff;
                            term.coeff = 0;
                            break;
                        case State::False:
                            term.coeff = 0;
                            break;
                        default:
                            break;
                    }
                    return term.coeff == 0;
                }),
            this->terms.end());
    }

    size_t mem() const {
        return sizeof(FixedSizeInequality<T>) + terms.size() * sizeof(Term<T>);
    }
};

template<typename T>
inline std::ostream& operator<<(std::ostream& os, const FixedSizeInequality<T>& v) {
    for (const Term<T>& term: v) {
        os << term << " + ";
    };
    os << " >= " << v.degree;
    return os;
}

struct PropState {
    size_t qhead = 0;
    bool conflict = false;
};

template<typename T>
class FixedSizeInequalityHandler {
private:
    void* malloc(size_t size) {
        capacity = size;
        size_t extra = extra_size_requirement<
                typename FixedSizeInequality<T>::TVec,
                typename FixedSizeInequality<T>::TElement
            >::value(size);
        size_t memSize = sizeof(FixedSizeInequality<T>) + extra;
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

    FixedSizeInequalityHandler(const FixedSizeInequality<T>& copyFrom) {
        void* addr = malloc(copyFrom.terms.size());
        ineq = new (addr) FixedSizeInequality<T>(copyFrom);
    }

    FixedSizeInequalityHandler(const FixedSizeInequalityHandler<T>& other):
        FixedSizeInequalityHandler(*other.ineq) {}

    FixedSizeInequalityHandler(FixedSizeInequalityHandler<T>&& other) {
        ineq = other.ineq;
        other.ineq = nullptr;
    }

    FixedSizeInequality<T>& operator*(){
        return *ineq;
    }

    FixedSizeInequality<T>* operator->(){
        return ineq;
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

    bool updateWatch = true;
    size_t dbMem = 0;
    size_t cumDbMem = 0;
    size_t maxDbMem = 0;


    class AutoReset {
    private:
        PropEngine& engine;
        PropState base;
    public:
        AutoReset(PropEngine& engine_)
            : engine(engine_)
            , base(engine.current) {

        }

        ~AutoReset(){
            engine.reset(base);
        }
    };

public:
    Assignment assignment;
    Assignment phase;
    LitIndexedVec<WatchList> watchlist;
    LitIndexedVec<OccursList> occurs;
    LitIndexedVec<Inequality<T>*> unattached;

    long long visit = 0;
    long long visit_sat = 0;
    long long visit_required = 0;


    PropEngine(size_t _nVars)
        : nVars(_nVars)
        , assignment(_nVars)
        , phase(_nVars)
        , watchlist(2 * (_nVars + 1))
        , occurs(2 * (_nVars + 1))
    {
        for (auto& ws: watchlist) {
            ws.reserve(50);
        }
        trail.reserve(nVars);
    }

    void printStats() {
        std::cout << "c statistic: used database memory: "
            << std::fixed << std::setprecision(3)
            << static_cast<float>(dbMem) / 1024 / 1024 / 1024 << " GB" << std::endl;

        std::cout << "c statistic: cumulative database memory: "
            << std::fixed << std::setprecision(3)
            << static_cast<float>(cumDbMem) / 1024 / 1024 / 1024 << " GB" << std::endl;

        std::cout << "c statistic: maximal used database memory: "
            << std::fixed << std::setprecision(3)
            << static_cast<float>(maxDbMem) / 1024 / 1024 / 1024 << " GB" << std::endl;

        std::cout << "c statistic: visit: " << visit << std::endl;
        std::cout << "c statistic: visit_sat: " << visit_sat << std::endl;
        std::cout << "c statistic: visit_required: " << visit_required << std::endl;
    }

    void enqueue(Lit lit) {
        assignment.assign(lit);
        phase.assign(lit);
        trail.push_back(lit);
    }

    void increaseNumVarsTo(size_t _nVars){
        assert(nVars <= _nVars);
        if (nVars < _nVars) {
            this->nVars = _nVars;
            assignment.resize(_nVars);
            phase.resize(_nVars);
            watchlist.resize(2 * (_nVars + 1));
            occurs.resize(2 * (_nVars + 1));
        }
    }

    void propagate() {
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
                    keepWatch = next->ineq->template updateWatch<false>(*this, falsifiedLit);
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

    bool checkSat(std::vector<int>& lits) {
        AutoReset autoReset(*this);
        attachUnattached();
        for (int lit: lits) {
            Lit l(lit);
            auto val = assignment.value[l];

            if (val == State::Unassigned) {
                enqueue(l);
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

        return success;
    }

    void _attach(Inequality<T>* ineq) {
        ineq->freeze(this->nVars);
        ineq->updateWatch(*this);
        propagate();
    }

    void attach(Inequality<T>* ineq) {
        if (!ineq->isAttached) {
            ineq->isAttached = true;
            dbMem += ineq->mem();
            cumDbMem += ineq->mem();
            maxDbMem = std::max(dbMem, maxDbMem);
            unattached.push_back(ineq);
        }
    }

    void attachUnattached() {
        for (Inequality<T>* ineq: unattached) {
            if (ineq != nullptr) {
                _attach(ineq);
            }
        }
        unattached.clear();
    }

    void detach(Inequality<T>* ineq) {
        if (ineq != nullptr) {
            if (ineq->isAttached) {
                ineq->isAttached = false;

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
    }

    /*
     * Propagates a constraint once without attaching it.
     * Returns true if some variable was propagated.
     */
    bool singleTmpPropagation(FixedSizeInequality<T>& ineq) {
        disableWatchUpdate();
        ineq.template updateWatch<true>(*this);
        enableWatchUpdate();

        return (current.qhead != trail.size());
    }

    void disableWatchUpdate(){
        this->updateWatch = false;
    }

    void enableWatchUpdate(){
        this->updateWatch = true;
    }

    bool ratCheck(const FixedSizeInequality<T>& redundant, const std::vector<Lit>& w) {
        attachUnattached();
        Assignment a(this->nVars);
        if (w.size() > 0) {
            for (Lit lit: w) {
                a.assign(lit);
            }
            if (!redundant.isSatisfied(a)) {
                // std::cout << "given assignment does not satisfy constraint" << std::endl;
                return false;
            }
        }

        AutoReset autoReset(*this);
        FixedSizeInequalityHandler<T> negated(redundant);
        negated->negate();

        while (!current.conflict &&
                singleTmpPropagation(*negated))
        {
            propagate();
        }

        if (current.conflict) {
            // std::cout << "rup check succeded" << std::endl;
            return true;
        } else if (w.size() == 0) {
            // std::cout << "rup check failed" << std::endl;
            return false;
        }

        for (Lit lit: w) {
            for (const FixedSizeInequality<T>* ineq: occurs[~lit]) {
                FixedSizeInequalityHandler<T> implied(*ineq);
                implied->restrictBy(a);

                // std::cout << "checking constraint" << std::endl;
                if (negated->implies(*implied)) {
                    // std::cout << "  ...syntactic implication check succeded" << std::endl;
                    continue;
                } else {
                    AutoReset autoReset(*this);
                    implied->negate();

                    while (!current.conflict
                        && (singleTmpPropagation(*implied)
                            || singleTmpPropagation(*negated)))
                    {
                        propagate();
                    }

                    if (!current.conflict) {
                        // std::cout << "  ...rup check failed" << std::endl;
                        return false;
                    } else {
                        // std::cout << "  ...rup check succeded" << std::endl;
                        continue;
                    }
                }
            }
        }

        // std::cout << "checking all implications succeded" << std::endl;
        return true;
    }

    void reset(PropState base) {
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
        if (this->updateWatch) {
            watchlist[lit].push_back(w);
        }
    }

    void addOccurence(Lit lit, FixedSizeInequality<T>& w) {
        if (this->updateWatch) {
            occurs[lit].push_back(&w);
        }
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
        for (Term<T>& term: ineq.terms) {
            T coeff = cpsign(term.coeff, term.lit);
            using namespace std;

            setCoeff(term.lit.var(), coeff);
        }
    }

    void unload(FixedSizeInequality<T>& ineq) {
        bussy = false;

        auto pos = ineq.terms.begin();
        for (Var var: this->usedList) {
            T coeff = this->coeffs[var];
            Lit lit(var, coeff < 0);
            using namespace std;
            coeff = abs(coeff);

            if (coeff > 0) {
                assert(pos != ineq.terms.end());
                pos->coeff = coeff;
                pos->lit = lit;
                pos += 1;
            }

            this->coeffs[var] = 0;
            this->used[var] = false;
        }
        assert(pos == ineq.terms.end());
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
        for (const Term<T> &term:other.terms) {
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
    bool isAttached = false;

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

        for (Term<T> term: ineq->terms) {
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

        for (Term<T>& term: ineq->terms) {
            using namespace std;
            term.coeff = min(term.coeff, ineq->degree);
        }

        if (ineq->degree <= 0 && ineq->terms.size() > 0) {
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
        for (Term<T>& term: ineq->terms) {
            term.coeff = divideAndRoundUp(term.coeff, divisor);
        }
        return this;
    }

    Inequality* multiply(T factor){
        assert(!frozen);
        contract();
        ineq->degree *= factor;
        for (Term<T>& term: ineq->terms) {
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
        std::vector<Term<T>> mine(ineq->terms.begin(), ineq->terms.end());
        sort(mine.begin(), mine.end(), orderByVar<Term<T>>);
        std::vector<Term<T>> theirs(other->ineq->terms.begin(), other->ineq->terms.end());
        sort(theirs.begin(), theirs.end(), orderByVar<Term<T>>);
        return (mine == theirs) && (ineq->degree == other->ineq->degree);
    }

    std::string toString(std::function<std::string(int)> varName) {
        contract();
        std::stringstream s;
        for (Term<T> &term: ineq->terms) {
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

        return ineq->implies(*other->ineq);
    }

    bool ratCheck(const std::vector<int>& w, PropEngine<T>& propEngine) {
        this->contract();
        std::vector<Lit> v;
        for (int i : w) {
            v.emplace_back(i);
        }
        return propEngine.ratCheck(*this->ineq, v);
    }

    Inequality* negated() {
        assert(!frozen);
        this->contract();
        ineq->negate();

        return this;
    }

    Inequality* copy(){
        contract();
        return new Inequality(*this);
    }

    bool isContradiction(){
        contract();
        T slack = -ineq->degree;
        for (Term<T> term: ineq->terms) {
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
