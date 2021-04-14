#include <vector>
#include <iostream>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <chrono>
#include <algorithm>
#include <functional>
#include <cstdlib>
#include <iomanip>
#include <type_traits>

#include "BigInt.hpp"

using CoefType = int;
// use one of the following lines istead to increas precision.
// using CoefType = long long;
// using CoefType = BigInt;

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

extern int hashColision;


template<typename T>
struct PointedEq {
    bool operator () ( T const * lhs, T const * rhs ) const {
        bool result = (*lhs == *rhs);
        if (!result) {
            hashColision += 1;
        }
        return result;
    }
};

template<typename T>
struct PointedHash {
    std::size_t operator () ( T const * element ) const {
        return std::hash<T>()(*element);
    }
};

template<typename T>
T divideAndRoundUp(T value, T divisor) {
    return (value + divisor - 1) / divisor;
}

typedef uint32_t LitData;

class Var {
public:
    static const LitData LIMIT = std::numeric_limits<LitData>::max() >> 1;

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

    friend std::hash<Lit>;
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

    bool operator<(const Lit& other) const {
        return value < other.value;
    }

    explicit operator size_t() const {
        return value;
    }

    explicit operator int64_t() const {
        int result = var();
        assert(result != 0);
        if (isNegated()) {result = -result;}
        return result;
    }

    static Lit Undef() {
        return Lit(0);
    };
};

namespace std {
    template <>
    struct hash<Lit> {
        std::size_t operator()(const Lit& lit) const {
            return hash<LitData>()(lit.value);
        }
    };
}

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

class Substitution {
public:
    static Lit zero() {
        // note that variable 0 is not a valid variable, hence we use
        // it to represent constants 0 as literal ~Var(0) and 1 as
        // literal Var(0). You may not swap the values of isNegated
        // passed to the constructor!

        return Lit(Var(0), true);
    }

    static Lit one() {
        return Lit(Var(0), false);
    }

    std::unordered_map<Lit, Lit> map;
    // var -> lit or 0 or 1

    Substitution(
        std::vector<int>& constants,
        std::vector<int>& from,
        std::vector<int>& to)
    {
        assert(from.size() == to.size());
        map.reserve(constants.size() + from.size());
        for (int intLit : constants) {
            Lit lit(intLit);
            map.emplace( lit, Substitution::one());
            map.emplace(~lit, Substitution::zero());
        }
        for (size_t i = 0; i < from.size(); i++) {
            Lit a(from[i]);
            Lit b(to[i]);
            map.emplace( a, b);
            map.emplace(~a,~b);
        }
    }
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
    return b.lit > a.lit;
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

template <typename T>
using InequalityPtr = std::unique_ptr<Inequality<T>>;

template<typename T>
class FixedSizeInequality;

class Clause;

template<typename T>
class ConstraintHandler;

template<typename T>
using FixedSizeInequalityHandler = ConstraintHandler<FixedSizeInequality<T>>;

using ClauseHandler = ConstraintHandler<Clause>;

template<typename T>
class PropEngine;

template<typename T>
struct WatchInfo {
    T* ineq;
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

    // warning requires custom allocation, if you know what you are
    // doing add a friend
    InlineVec(size_t size)
        : _size(size) {};

    template<typename T>
    friend class FixedSizeInequality;
    friend class Clause;
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

class ClausePropagator {
public:
    using WatchedType = WatchInfo<Clause>;
    using WatchList = std::vector<WatchedType>;

    Assignment& assignment;
    std::vector<Lit>& trail;
    LitIndexedVec<WatchList> watchlist;

    void enqueue(Lit lit) {
        // todo
    }

    void conflict() {

    }

    void watch(Lit lit, WatchedType& w) {
        watchlist[lit].push_back(w);
    }
};

class Clause {
private:
    using TElement = Lit;
    using TVec = InlineVec<TElement>;

    friend ClauseHandler;

public:
    size_t propagationSearchStart = 2;
    InlineVec<Lit> terms;

private:
    Clause(size_t size)
        :terms(size) {
    }

    Clause(const Clause& other)
        : terms(other.terms.size())
    {
        using namespace std;
        copy(other.terms.begin(), other.terms.end(), this->terms.begin());
    }

    Clause(std::vector<Lit>& _terms)
        : terms(_terms.size())
    {
        std::copy(_terms.begin(),_terms.end(), this->terms.begin());
    }

    void checkPosition(size_t pos, int &numFound, size_t (&watcher)[2], Assignment& assignment) {
        Lit& lit = this->terms[pos];
        switch(assignment[lit]) {
            case State::True:
                // watcher[numFound] = pos;
                // numFound = 2;
                // break;
            case State::Unassigned:
                watcher[numFound] = pos;
                numFound += 1;
                break;
            case State::False:
                break;
        }
    }
public:
    void updateWatch(ClausePropagator& prop, Lit falsifiedLit, bool initial = false) {
        int numFound = 0;
        // watcher will contain the position of the literals to be watched
        size_t watcher[2];
        watcher[0] = 0;
        watcher[1] = 1;

        if (this->terms.size() >= 2) {
            if (this->terms[1] == falsifiedLit) {
                std::swap(this->terms[1], this->terms[0]);
            }

            checkPosition(0, numFound, watcher, prop.assignment);

            if (numFound < 2) {
                checkPosition(1, numFound, watcher, prop.assignment);
            }

            size_t pos = this->propagationSearchStart;
            for (size_t i = 0; i < this->terms.size() - 2 && numFound < 2; i++) {
                checkPosition(pos, numFound, watcher, prop.assignment);
                pos += 1;
                if (pos == this->terms.size()) {
                    pos = 2;
                }
            }
            this->propagationSearchStart = pos;
        } else if (this->terms.size() == 1) {
            checkPosition(0, numFound, watcher, prop.assignment);
        }

        // LOG(debug) << "numFound: "<< numFound << EOM;
        if (numFound == 1) {
            Lit lit = this->terms[watcher[0]];
            if (prop.assignment[lit] == State::Unassigned) {
                prop.enqueue(lit);
            }
        }

        if (numFound == 0) {
            prop.conflict();
        }

        if (this->terms.size() >= 2) {
            // This variables controlles if we want to update all
            // watches (2) or just the falsified watch (1). The
            // problem with updating all watches is that it might
            // cause memory blowup unless we remove the swaped out
            // watcher.
            const size_t numUpdatedWatches = 1;
            for (size_t i = 0; i < numUpdatedWatches; i++) {
                auto& lits = this->terms;
                std::swap(lits[i], lits[watcher[i]]);
                if (watcher[i] >= 2 || initial || lits[i] == falsifiedLit) { // was not a watched literal / needs to be readded
                    WatchInfo<Clause> watchInfo;
                    watchInfo.ineq = this;
                    watchInfo.other = lits[watcher[(i + 1) % 2]];
                    prop.watch(lits[i], watchInfo);
                }
            }
            if (numUpdatedWatches == 1 && numFound == 2 && watcher[1] >= 2) {
                // Let us start the search where we found the second
                // non falsified literal.
                this->propagationSearchStart = watcher[1];
            }
        }
    }
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
    friend std::hash<FixedSizeInequality<T>>;

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

        WatchInfo<FixedSizeInequality<T>> w;
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
        }

        // use statick variable to avoid reinizialisation for BigInts,
        // prevents prallel or recursive execution!
        static T slack;
        slack = -this->degree;

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
                    WatchInfo<FixedSizeInequality<T>> w;
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

    void substitute(Substitution& sub) {
        // warning: after this operation the constraint is no longer
        // normalized, load to FatInequality to normalize
        for (Term<T>& term: this->terms) {
            auto it = sub.map.find(term.lit);
            if (it != sub.map.end()) {
                term.lit = it->second;
            }
        }
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

    void restrictByFalseLits(const Assignment& a){
        this->terms.erase(
            std::remove_if(
                this->terms.begin(),
                this->terms.end(),
                [this, &a](Term<T>& term){
                    switch (a[term.lit]) {
                        case State::True:
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

namespace std {
    template <typename T>
    struct hash<FixedSizeInequality<T>> {
        std::size_t operator()(const FixedSizeInequality<T>& ineq) const {
            std::vector<Term<T>> mine(ineq.terms.begin(), ineq.terms.end());
            sort(mine.begin(), mine.end(), orderByVar<Term<T>>);

            std::size_t seed = 0;

            for (const Term<T>& term: mine) {
                hash_combine(seed, term.coeff);
                hash_combine(seed, term.lit);
            }

            hash_combine(seed, ineq.degree);
            return seed;
        }
    };
}

template<typename T>
inline std::ostream& operator<<(std::ostream& os, const FixedSizeInequality<T>& v) {
    for (const Term<T>& term: v.terms) {
        os << term << " ";
    };
    os << " >= " << v.degree;
    return os;
}

struct PropState {
    size_t qhead = 0;
    bool conflict = false;
};

template<typename TConstraint>
class ConstraintHandler {
private:
    void* malloc(size_t size) {
        capacity = size;
        size_t extra = extra_size_requirement<
                typename TConstraint::TVec,
                typename TConstraint::TElement
            >::value(size);
        size_t memSize = sizeof(TConstraint) + extra;
        // return std::aligned_alloc(64, memSize);
        return std::malloc(memSize);
    }

    void free(){
        if (ineq != nullptr) {
            ineq->~TConstraint();
            std::free(ineq);
            ineq = nullptr;
        }
    }

public:
    TConstraint* ineq = nullptr;
    size_t capacity = 0;

    ConstraintHandler(const TConstraint& copyFrom) {
        void* addr = malloc(copyFrom.terms.size());
        ineq = new (addr) TConstraint(copyFrom);
    }

    ConstraintHandler(const ConstraintHandler<TConstraint>& other):
        ConstraintHandler(*other.ineq) {}

    ConstraintHandler(ConstraintHandler<TConstraint>&& other) {
        ineq = other.ineq;
        other.ineq = nullptr;
    }

    TConstraint& operator*(){
        return *ineq;
    }

    TConstraint* operator->(){
        return ineq;
    }

    ConstraintHandler& operator=(ConstraintHandler&& other) {
        this->free();
        ineq = other.ineq;
        other.ineq = nullptr;
    }

    template <typename ...Args>
    ConstraintHandler(size_t size, Args && ...args) {
        void* addr = malloc(size);
        ineq = new (addr) TConstraint(std::forward<Args>(args)...);
    }

    // ConstraintHandler(std::vector<Term<T>>& terms, T degree) {
    //     void* addr = malloc(terms.size());
    //     ineq = new (addr) TConstraint(terms, degree);
    // }

    void resize(size_t size) {
        if (ineq != nullptr && size == capacity) {
            return;
        }
        free();
        void* addr = malloc(size);
        ineq = new (addr) TConstraint(size);
    }

    ~ConstraintHandler(){
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
    typedef WatchInfo<FixedSizeInequality<T>> WatchedType;
    typedef std::vector<WatchedType> WatchList;
    typedef std::unordered_set<Inequality<T>*>  OccursList;

    typedef Inequality<T> Ineq;
    std::unordered_set<Ineq*, PointedHash<Ineq>, PointedEq<Ineq>> constraintLookup;

    size_t nVars;
    std::vector<Lit> trail;

    PropState current;

    bool updateWatch = true;
    size_t dbMem = 0;
    size_t cumDbMem = 0;
    size_t maxDbMem = 0;


    /*
     * Automatically resets the trail to its previous state after the
     * scope is left. Requires that only new things were put on the
     * trail, i.e., the trail did not grow shorter.
     */
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
    std::vector<Inequality<T>*> unattached;
    std::vector<Inequality<T>*> propagatingAt0;
    std::chrono::duration<double> timeEffected;
    std::chrono::duration<double> timeFind;
    std::chrono::duration<double> timeInitProp;


    long long visit = 0;
    long long visit_sat = 0;
    long long visit_required = 0;
    long long lookup_requests = 0;


    PropEngine(size_t _nVars)
        : nVars(_nVars)
        , assignment(_nVars)
        , phase(_nVars)
        , watchlist(2 * (_nVars + 1))
        , occurs(2 * (_nVars + 1))
        , timeEffected(0)
        , timeFind(0)
        , timeInitProp(0)
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

        std::cout << "c statistic: time effected: "
            << std::fixed << std::setprecision(2)
            << timeEffected.count() << std::endl ;

        std::cout << "c statistic: time find: "
            << std::fixed << std::setprecision(2)
            << timeFind.count() << std::endl ;

        std::cout << "c statistic: time initpropagation: "
            << std::fixed << std::setprecision(2)
            << timeInitProp.count() << std::endl ;

        std::cout << "c statistic: hashColisions: " << hashColision << std::endl;
        std::cout << "c statistic: lookup_requests: " << lookup_requests << std::endl;
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

    bool propagate4sat(std::vector<int>& lits) {
        AutoReset autoReset(*this);

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

    bool checkSat(std::vector<int>& lits) {
        AutoReset reset(*this);
        initPropagation();
        return propagate4sat(lits);
    }

    void _attach(Inequality<T>* ineq) {
        AutoReset autoReset(*this);

        assert(this->trail.size() == 0);
        ineq->updateWatch(*this);
        if (this->trail.size() > 0 || isConflicting()) {
            this->propagatingAt0.push_back(ineq);
        }
    }

    Inequality<T>* attach(Inequality<T>* toAttach, uint64_t id) {
        Inequality<T>* ineq;
        {
            Timer timer(timeFind);
            ineq = *constraintLookup.insert(toAttach).first;
            lookup_requests += 1;
        }

        ineq->ids.insert(id);
        ineq->minId = std::min(ineq->minId, id);

        if (!ineq->isAttached) {
            ineq->isAttached = true;
            ineq->freeze(this->nVars);
            ineq->registerOccurence(*this);
            dbMem += ineq->mem();
            cumDbMem += ineq->mem();
            maxDbMem = std::max(dbMem, maxDbMem);
            unattached.push_back(ineq);
        }
        return ineq;
    }

    void initPropagation() {
        Timer timer(timeInitProp);
        assert(this->current.qhead == 0);
        attachUnattached();
        for (Inequality<T>* ineq: this->propagatingAt0) {
            ineq->updateWatch(*this);
        }
        // propagate();
    }

    void attachUnattached() {
        for (Inequality<T>* ineq: unattached) {
            if (ineq != nullptr) {
                _attach(ineq);
            }
        }
        unattached.clear();
    }

    int attachCount(Inequality<T>* ineq) {
        Inequality<T>* tmp;
        assert( (tmp = find(ineq), tmp == nullptr || tmp == ineq) );
        return ineq->ids.size() - ineq->detachCount;
    }

    std::vector<uint64_t> getDeletions(Inequality<T>* ineq) {
        std::vector<uint64_t> result;

        Inequality<T>* attachedIneq = find(ineq);
        if (attachedIneq) {
            attachedIneq->detachCount += 1;
            if (attachedIneq->ids.size() <= attachedIneq->detachCount) {
                std::copy(attachedIneq->ids.begin(), attachedIneq->ids.end(), std::back_inserter(result));
                attachedIneq->ids.clear();
                attachedIneq->detachCount = 0;
            }
        }

        return result;
    }

    void detach(Inequality<T>* ineq, uint64_t id) {
        if (ineq != nullptr) {
            ineq->ids.erase(id);
            if (ineq->minId == id && ineq->ids.size() > 0) {
                ineq->minId = *std::min_element(ineq->ids.begin(), ineq->ids.end());
            }

            if (ineq->isAttached && ineq->ids.size() == 0) {
                ineq->isAttached = false;
                ineq->unRegisterOccurence(*this);
                constraintLookup.erase(ineq);
                lookup_requests += 1;

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

    std::vector<int> propagatedLits() {
        AutoReset reset(*this);
        initPropagation();
        propagate();

        std::vector<int> assignment;
        for (uint var = 1; var <= nVars; var++) {
            State val = this->assignment.value[Lit(var)];
            int lit = var;
            if (val != State::Unassigned) {
                if (val == State::True) {
                    assignment.push_back(lit);
                } else {
                    assignment.push_back(-lit);
                }
            }
        }
        return assignment;
    }

    bool rupCheck(const FixedSizeInequality<T>& redundant) {
        AutoReset reset(*this);
        initPropagation();

        FixedSizeInequalityHandler<T> negated(redundant);
        negated->negate();

        while (!current.conflict &&
                singleTmpPropagation(*negated))
        {
            propagate();
        }

        // for (Lit lit : this->trail) {
        //     std::cout << lit << " ";
        // }
        // std::cout << std::endl;
        return current.conflict;
    }

    long long estimateNumEffected(Substitution& sub) {
        long long estimate = 0;
        for (auto it: sub.map) {
            Lit from = it.first;
            Lit to   = it.second;

            if (to != Substitution::one()) {
                estimate += occurs[from].size();
            }
        }
        return estimate;
    }

    std::vector<InequalityPtr<T>> computeEffected(
            Substitution& sub,
            uint64_t includeIds = std::numeric_limits<uint64_t>::max())
    {
        Timer timer(timeEffected);
        std::unordered_set<Inequality<T>*> unique;
        for (auto it: sub.map) {
            Lit from = it.first;
            Lit to   = it.second;

            // constraints are normalized, if a literal is set to
            // one then this is the same as weakening, hence we do
            // not need to add it to the set of effected
            // constraints
            if (to != Substitution::one()) {
                unique.insert(occurs[from].begin(), occurs[to].end());
            }
        }

        std::vector<InequalityPtr<T>> result;

        for (Inequality<T>* ineq: unique) {
            if (ineq->minId <= includeIds) {
                InequalityPtr<T> rhs(ineq->copy());
                rhs->substitute(sub);
                if (!ineq->implies(rhs.get()) && !find(rhs.get())
                    ) {
                    result.emplace_back(std::move(rhs));
                }
            }
        }

        return result;
    }

    Inequality<T>* find(Inequality<T>* ineq) {
        Timer timer(timeFind);
        ineq->contract();
        lookup_requests += 1;

        auto result = constraintLookup.find(ineq);
        if (result == constraintLookup.end()) {
            return nullptr;
        } else {
            return *result;
        }
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

    void watch(Lit lit, WatchedType& w) {
        if (this->updateWatch) {
            watchlist[lit].push_back(w);
        }
    }

    void addOccurence(Lit lit, Inequality<T>& w) {
        if (this->updateWatch) {
            occurs[lit].emplace(&w);
        }
    }

    void rmOccurence(Lit lit, Inequality<T>& w) {
        if (this->updateWatch) {
            occurs[lit].erase(&w);
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
            if (term.coeff < 0) {
                term.lit = ~term.lit;
                term.coeff = -term.coeff;
                this->degree += term.coeff;
            }
            addLhs(term);
            // T coeff = cpsign(term.coeff, term.lit);
            // using namespace std;

            // setCoeff(term.lit.var(), coeff);
        }

        if (this->coeffs.size() > 0) {
            Var one = Substitution::one().var();
            T& coeff = this->coeffs[one];
            if (coeff > 0) {
                this->degree -= coeff;
            }
            this->coeffs[one] = 0;
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
                assert(var != Substitution::one().var());
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

    void weaken(Var var) {
        this->degree -= abs(this->coeffs[var]);
        this->coeffs[var] = 0;
    }

    /* requires positive coefficients */
    void addLhs(const Term<T> &term) {
        using namespace std;
        T b = cpsign(term.coeff, term.lit);
        Var var = term.lit.var();
        this->use(var);
        T a = this->coeffs[var];
        this->coeffs[var] = a + b;
        T cancellation = max<T>(0, max(abs(a), abs(b)) - abs(this->coeffs[var]));
        this->degree -= cancellation;
    }

    void add(const FixedSizeInequality<T>& other) {
        for (const Term<T> &term:other.terms) {
            addLhs(term);
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

    friend PropEngine<T>;
    friend std::hash<Inequality<T>>;
public:
    bool isAttached = false;
    uint64_t minId = std::numeric_limits<uint64_t>::max();
    std::unordered_set<uint64_t> ids;
    uint detachCount = 0;

    Inequality(Inequality& other)
        : handler(other.handler)
    {
        assert(other.loaded == false);
        this->minId = other.minId;
    }

    Inequality(std::vector<Term<T>>& terms_, T degree_)
        : handler(terms_.size(), terms_, degree_)
    {
        expand();
        contract();
    }

    Inequality(std::vector<Term<T>>&& terms_, T degree_)
        : handler(terms_.size(), terms_, degree_)
    {
        expand();
        contract();
    }

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

        auto& propagating = prop.propagatingAt0;
        propagating.erase(
            std::remove_if(
                propagating.begin(),
                propagating.end(),
                [this](auto& ineq){
                    return ineq == this;
                }
            ),
            propagating.end()
        );

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
        return *this == *other;
    }

    bool operator==(Inequality<T>& other) {
        contract();
        other.contract();
        std::vector<Term<T>> mine(ineq->terms.begin(), ineq->terms.end());
        sort(mine.begin(), mine.end(), orderByVar<Term<T>>);
        std::vector<Term<T>> theirs(other.ineq->terms.begin(), other.ineq->terms.end());
        sort(theirs.begin(), theirs.end(), orderByVar<Term<T>>);
        return (mine == theirs) && (ineq->degree == other.ineq->degree);
    }

    bool operator==(const Inequality<T>& other) const {
        assert(!this->loaded);
        assert(!other.loaded);
        std::vector<Term<T>> mine(ineq->terms.begin(), ineq->terms.end());
        sort(mine.begin(), mine.end(), orderByVar<Term<T>>);
        std::vector<Term<T>> theirs(other.ineq->terms.begin(), other.ineq->terms.end());
        sort(theirs.begin(), theirs.end(), orderByVar<Term<T>>);
        return (mine == theirs) && (ineq->degree == other.ineq->degree);
    }

    bool operator!=(Inequality<T>& other) {
        return !(*this == other);
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

    bool rupCheck(PropEngine<T>& propEngine) {
        this->contract();
        return propEngine.rupCheck(*this->ineq);
    }

    Inequality* substitute(Substitution& sub) {
        assert(!this->frozen);
        this->contract();
        this->ineq->substitute(sub);
        // need to normalize, we do so by expanding the constraint
        this->expand();
        return this;
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

    Inequality* weaken(int _var) {
        assert(_var >= 0);
        Var var(_var);
        expand();
        expanded->weaken(var);
        return this;
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

    void registerOccurence(PropEngine<T>& prop) {
        assert(frozen);
        for (Term<T>& term: this->ineq->terms) {
            prop.addOccurence(term.lit, *this);
        }
    }

    void unRegisterOccurence(PropEngine<T>& prop) {
        assert(frozen);
        for (Term<T>& term: this->ineq->terms) {
            prop.rmOccurence(term.lit, *this);
        }
    }
};

namespace std {
    template <typename T>
    struct hash<Inequality<T>> {
        std::size_t operator()(const Inequality<T>& ineq) const {
            return std::hash<FixedSizeInequality<T>>()(*ineq.ineq);
        }
    };
}

// we need to initialzie the static template member manually;
template<typename T>
std::vector<FatInequalityPtr<T>> Inequality<T>::pool;
