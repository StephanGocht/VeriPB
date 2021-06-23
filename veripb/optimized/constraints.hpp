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

#ifndef NDEBUG
    #include <unistd.h>
    #include <execinfo.h>
#endif

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

    #ifndef NDEBUG
        void *array[10];
        size_t size;

        // get void*'s for all entries on the stack
        size = backtrace(array, 10);

        // print out all the frames to stderr
        backtrace_symbols_fd(array, size, STDERR_FILENO);
    #endif

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

class IJunkyard {
public:
    virtual void clear() = 0;
};

class MasterJunkyard {
public:
    std::vector<IJunkyard*> yards;

    static MasterJunkyard& get() {
        static MasterJunkyard junkyard;
        return junkyard;
    }

    void clearAll() {
        for (IJunkyard* yard: yards) {
            yard->clear();
        }
    }
};

template<typename T>
class Junkyard : public IJunkyard {
private:
    std::vector<T> junk;

public:
    Junkyard(MasterJunkyard& master) {
        master.yards.push_back(this);
    }

    static Junkyard<T>& get() {
        static Junkyard<T> junkyard(MasterJunkyard::get());
        return junkyard;
    }

    void add(T&& value) {
        junk.push_back(std::move(value));
    }

    void clear() {
        junk.clear();
    }
};

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
        size_t pos = static_cast<size_t>(idx);
        assert(pos < this->size());
        return this->std::vector<T>::operator[](pos);
    }
    const T& operator[](Lit idx) const {
        size_t pos = static_cast<size_t>(idx);
        assert(pos < this->size());
        return this->std::vector<T>::operator[](pos);
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
        size_t pos = static_cast<size_t>(idx);
        assert(pos < this->size());
        return this->std::vector<T>::operator[](pos);
    }
    const T& operator[](Var idx) const {
        size_t pos = static_cast<size_t>(idx);
        assert(pos < this->size());
        return this->std::vector<T>::operator[](pos);
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
    using coeff_type = T;

    T coeff;
    Lit lit;

    Term() = default;

    Term(T coeff_, Lit lit_)
        : coeff(coeff_)
        , lit(lit_)
        {}

    template<typename TTermType>
    Term<T>& operator=(const Term<TTermType>& other) {
        coeff = other.coeff;
        lit = other.lit;
        return *this;
    }

    template<typename TTerm>
    bool operator==(const TTerm& other) const {
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

template<>
struct Term<void> {
    using coeff_type = int8_t;

    Lit lit;
    static constexpr int8_t coeff = 1;

    Term() = default;

    template<typename T>
    Term(T _coeff, Lit _lit)
        : lit(_lit)
    {
        assert(_coeff == 1);
    }

    Term<void>& operator=(const Lit& _lit) {
        lit = _lit;
        return *this;
    }

    template<typename T>
    Term<void>& operator=(const Term<T>& other) {
        assert(other.coeff == 1);
        lit = other.lit;
        return *this;
    }


    template<typename TTerm>
    bool operator==(const TTerm& other) const {
        return (this->coeff == other.coeff) && (this->lit == other.lit);
    }
};

// While this is not reqired for correctness, we do want that clauses
// only take up space for terms and not for coefficients.
static_assert(sizeof(Term<void>) == sizeof(Lit));

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

struct PropState {
    size_t qhead = 0;
    bool conflict = false;
};

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

    const Element& operator[](size_t index) const {
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

struct DBConstraintHeader {
    bool isMarkedForDeletion = false;
    bool isReason = false;
};


class Reason {
public:
    virtual void rePropagate() = 0;
    virtual bool isMarkedForDeletion() = 0;
    virtual void setIsReason() = 0;
    virtual void unsetIsReason() = 0;
    virtual ~Reason(){};
};

using ReasonPtr = std::unique_ptr<Reason>;

template<typename TConstraint, typename TPropagator>
class GenericDBReason: public Reason {
    TConstraint& constraint;
    TPropagator& propagator;

public:
    GenericDBReason(TConstraint& _constraint, TPropagator& _propagator)
        : constraint(_constraint)
        , propagator(_propagator)
    {}

    virtual void rePropagate() {
        // tdod: this may causing duplicate entries in the watch list
        // as init will always add an entry to the watch list no
        // matter if there was an existing entry before, maybe we
        // should have something like initWatch, updateAllWatches,
        // updateSingleWatch.
        constraint.updateWatch(propagator);
    }

    virtual bool isMarkedForDeletion() {
        return constraint.header.isMarkedForDeletion;
    }

    virtual void setIsReason() {
        constraint.header.isReason = true;
    }

    virtual void unsetIsReason() {
        constraint.header.isReason = false;
    }

    virtual ~GenericDBReason() {}
};


class Propagator {
protected:
    size_t qhead = 0;

public:
    virtual void propagate() = 0;
    virtual void cleanupWatches() = 0;
    virtual void increaseNumVarsTo(size_t) = 0;
    virtual void undo_trail_till(size_t pos) {
        if (qhead > pos)
            qhead = pos;
    };
    virtual ~Propagator(){};
};

using PropagatorPtr = std::unique_ptr<Propagator>;

class PropagationMaster {
private:
    Assignment assignment;
    Assignment phase;
    std::vector<Lit> trail;
    std::vector<ReasonPtr> reasons;


    PropState current;

    bool trailUnchanged = true;

    friend class AutoReset;

public:
    std::vector<Propagator*> propagators;
    std::chrono::duration<double> timeCleanupTrail;

    const Assignment& getAssignment() {return assignment;}
    const Assignment& getPhase() {return phase;}
    const std::vector<Lit>& getTrail() {return trail;}

    PropagationMaster(size_t nVars)
        : assignment(nVars)
        , phase(nVars)
    {
        trail.reserve(nVars);
    }

    void increaseNumVarsTo(size_t _nVars) {
        assignment.resize(_nVars);
        phase.resize(_nVars);
        for (Propagator* propagator: propagators) {
            propagator->increaseNumVarsTo(_nVars);
        }
    }

    void conflict() {
        current.conflict = true;
    }

    bool isConflicting() {
        return current.conflict;
    }

    void enqueue(Lit lit, ReasonPtr&& reason) {
        // std::cout << "Enqueueing: " << lit << std::endl;
        trailUnchanged = false;
        assignment.assign(lit);
        phase.assign(lit);
        trail.push_back(lit);
        reasons.emplace_back(std::move(reason));
        if (reasons.back().get() != nullptr) {
            reasons.back()->setIsReason();
        }

    }

    bool isTrailClean() {
        for (ReasonPtr& reason: reasons) {
            if (reason->isMarkedForDeletion()) {
                return false;
            }
        }

        return true;
    }

    void cleanupTrail() {
        Timer timer(timeCleanupTrail);
        if (isTrailClean()) {
            return;
        }

        std::vector<ReasonPtr> oldReasons;
        std::swap(oldReasons, reasons);
        std::vector<Lit> oldTrail;
        std::swap(oldTrail, trail);

        for (Lit lit: oldTrail) {
            assignment.unassign(lit);
        }

        PropState emptyTrail;
        reset(emptyTrail);

        for (size_t i = 0; i < oldTrail.size(); ++i) {
            ReasonPtr& reason = oldReasons[i];
            if (reason.get() != nullptr) {
                // deleted constraints are not supposed to propagate
                // so this will make sure that no deleted constraint
                // is left on the cleaned up trail.
                reason->rePropagate();
            } else {
                // nullptr reason can happen when we have decisions,
                // e.g., when checking if an assignment is
                // satisfiable.
                enqueue(oldTrail[i], nullptr);
            }
        }

        assert(isTrailClean());
    }

    void cleanupWatches() {
        for (Propagator* propagator: propagators) {
            propagator->cleanupWatches();
        }
    }

    void propagate() {
        while (current.qhead < trail.size()) {
            trailUnchanged = true;
            for (Propagator* propagator: propagators) {
                propagator->propagate();
                if (!trailUnchanged) {
                    break;
                }
            }
            if (trailUnchanged) {
                current.qhead = trail.size();
            }
        }
    }

    void undoOne() {
        assignment.unassign(trail.back());
        trail.pop_back();
        if (reasons.back().get() != nullptr) {
            reasons.back()->unsetIsReason();
        }
        reasons.pop_back();
    }

    void reset(PropState resetState) {
        // std::cout << "Reset to trail pos " << resetState.qhead << std::endl;
        for (Propagator* propagator: propagators) {
            propagator->undo_trail_till(resetState.qhead);
        }

        while (trail.size() > resetState.qhead) {
            undoOne();
        }

        current = resetState;
    }
};

class Clause;

class ClausePropagator: public Propagator {
public:
    using WatchedType = WatchInfo<Clause>;
    using WatchList = std::vector<WatchedType>;

    PropagationMaster& propMaster;
    LitIndexedVec<WatchList> watchlist;

    ClausePropagator(PropagationMaster& _propMaster, size_t nVars)
        : propMaster(_propMaster)
        , watchlist(2 * (nVars + 1))
    {

    }

    void watch(Lit lit, WatchedType& w) {
        // std::cout << "adding watch  " << lit << std::endl;
        watchlist[lit].push_back(w);
    }

    virtual void increaseNumVarsTo(size_t nVars) {
        watchlist.resize(2 * (nVars + 1));
    }

    virtual void propagate();
    virtual void cleanupWatches();
};

using ClauseReason = GenericDBReason<Clause, ClausePropagator>;

class Clause {
private:
    using TElement = Term<void>;
    using TVec = InlineVec<TElement>;

    friend ClauseHandler;
    friend std::ostream& operator<<(std::ostream& os, const Clause& v);

public:
    using TTerm = TElement;
    size_t propagationSearchStart = 2;

    // common constraint interface:
    DBConstraintHeader header;
    static constexpr int8_t degree = 1;
    TVec terms;

private:
    // size_t size() const {
    //     return terms.size();
    // }

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

    template<typename T>
    Clause(const FixedSizeInequality<T>& other)
        : terms(other.terms.size())
    {
        for (size_t i = 0; i < other.terms.size(); ++i) {
            terms[i].lit = other.terms[i].lit;
        }
    }

    void checkPosition(size_t pos, int &numFound, size_t (&watcher)[2], const Assignment& assignment) {
        Lit& lit = this->terms[pos].lit;
        switch(assignment[lit]) {
            case State::True:
                if (pos == 1) {
                    watcher[0] = 0;
                    watcher[1] = 1;
                } else {
                    watcher[0] = pos;
                    watcher[1] = 1;
                }
                numFound = 2;
                break;
            case State::Unassigned:
                watcher[numFound] = pos;
                numFound += 1;
                break;
            case State::False:
                break;
        }
    }
public:
    void clearWatches(ClausePropagator& prop) {
        size_t nWatcher = std::min((size_t) 2, terms.size());
        for (size_t i = 0; i < nWatcher; i++) {
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

    void initWatch(ClausePropagator& prop) {
        for (Term<void> term: terms) {
            assert(static_cast<size_t>(term.lit.var()) < prop.propMaster.getAssignment().value.size());
            assert(static_cast<size_t>(term.lit) < prop.watchlist.size());
        }
        // std::cout << "initializing " << *this << std::endl;
        updateWatch(prop, Lit::Undef(), true);
    }

    bool updateWatch(ClausePropagator& prop, Lit falsifiedLit = Lit::Undef(), bool initial = false) {
        bool keep = true;

        if (header.isMarkedForDeletion) {
            keep = false;
            return keep;
        }

        assert(falsifiedLit == Lit::Undef()
                || (this->terms.size() < 2
                    || (this->terms[0].lit == falsifiedLit
                        || this->terms[1].lit == falsifiedLit
            )));


        int numFound = 0;
        // watcher will contain the position of the terms to be watched
        size_t watcher[2];

        const Assignment& assignment = prop.propMaster.getAssignment();

        if (this->terms.size() >= 2) {
            if (this->terms[1].lit == falsifiedLit) {
                std::swap(this->terms[0], this->terms[1]);
            }

            checkPosition(0, numFound, watcher, assignment);
            if (numFound < 2) {
                checkPosition(1, numFound, watcher, assignment);
            }

            size_t pos = this->propagationSearchStart;
            for (size_t i = 0; i < this->terms.size() - 2 && numFound < 2; i++) {
                checkPosition(pos, numFound, watcher, assignment);
                pos += 1;
                if (pos == this->terms.size()) {
                    pos = 2;
                }
            }
            this->propagationSearchStart = pos;
        } else if (this->terms.size() == 1) {
            checkPosition(0, numFound, watcher, assignment);
        }

        // LOG(debug) << "numFound: "<< numFound << EOM;
        if (numFound == 1) {
            Lit lit = this->terms[watcher[0]].lit;
            if (assignment[lit] == State::Unassigned) {
                prop.propMaster.enqueue(
                    lit,
                    std::make_unique<ClauseReason>(*this, prop));
            }
        }

        if (numFound == 0) {
            prop.propMaster.conflict();
        }

        if (this->terms.size() >= 2) {
            if (numFound == 0) {
                watcher[0] = 0;
                watcher[1] = 1;
            } else if (numFound == 1) {
                if (watcher[0] == 1) {
                    watcher[0] = 0;
                    watcher[1] = 1;
                } else {
                    watcher[1] = 1;
                }
            } else if (watcher[0] == 1) {
                std::swap(watcher[0], watcher[1]);
            }

            if (initial || numFound > 0) {
                for (size_t i = 0; i < 2; i++) {
                    assert(watcher[i] < terms.size());

                    if (initial
                        || (i == 0
                            && watcher[i] >= 2
                            && falsifiedLit != Lit::Undef())) {
                        // was not a watched literal / needs to be readded
                        keep = false;
                        // assert(watcher[0] < watcher[1] || watcher[1] == 1);
                        assert(initial || (terms[i].lit == falsifiedLit));
                        std::swap(terms[i], terms[watcher[i]]);
                        WatchInfo<Clause> watchInfo;
                        watchInfo.ineq = this;
                        watchInfo.other = terms[watcher[(i + 1) % 2]].lit;
                        prop.watch(terms[i].lit, watchInfo);
                    }
                }
            }

            if (!initial && numFound == 2 && watcher[1] >= 2) {
                // Let us start the search where we found the second
                // non falsified literal.
                this->propagationSearchStart = watcher[1];
            }
        }
        return keep;
    }
};


inline std::ostream& operator<<(std::ostream& os, const Clause& v) {
    for (Term<void> term: v.terms) {
        os << term << " ";
    };
    os << " >= 1";
    return os;
}

template<typename T>
class IneqPropagator;

template<typename T>
using IneqReason = GenericDBReason<FixedSizeInequality<T>, IneqPropagator<T>>;

template<typename T>
class FixedSizeInequality {
private:
    uint32_t watchSize = 0;

    using TElement = Term<T>;
    using TVec = std::conditional_t<
            std::is_trivially_destructible<T>::value,
            InlineVec<TElement>,
            std::vector<TElement>
        >;
public:
    T maxCoeff = 0;

    using TTerm = TElement;

    // common constraint interface:
    DBConstraintHeader header;
    T degree;
    TVec terms;

private:
    friend FixedSizeInequalityHandler<T>;
    friend std::hash<FixedSizeInequality<T>>;

    size_t size() const {
        return terms.size();
    }

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

    template<typename TermIter, typename TInt>
    FixedSizeInequality(size_t size, TermIter begin, TermIter end, TInt _degree)
        : degree(_degree)
        , terms(size)
    {
        std::copy(begin, end, this->terms.begin());
    }

    // FixedSizeInequality(Clause& clause)
    //     : degree(1)
    //     , terms(clause.terms.size())
    // {
    //     std::copy(clause.terms.begin(),clause.terms.end(), this->terms.begin());
    //     // Term<T> term;
    //     // term.coeff = 1;
    //     // Term<T>* it = this->terms.begin();
    //     // for (Term<void> term: clause.terms) {
    //     //     term.lit = lit;
    //     //     *it = term;
    //     //     ++it;
    //     // }
    // }

    void computeMaxCoeff(){
        std::sort(terms.begin(), terms.end(), orderByCoeff<Term<T>>);
        maxCoeff = terms[terms.size() - 1].coeff;
    }

    void computeWatchSize() {
        if (terms.size() == 0) {
            return;
        }

        computeMaxCoeff();

        // we propagate lit[i] if slack < coeff[i] so we
        // need to make sure that if no watch is falsified we
        // have always slack() >= maxCoeff
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

    bool isPropagatingAt0() {
        if (terms.size() == 0) {
            return false;
        }

        if (maxCoeff == 0) {
            computeMaxCoeff();
        }

        T value = -this->degree;

        size_t i = 0;
        for (; i < terms.size(); i++) {
            value += terms[i].coeff;
            if (value >= maxCoeff) {
                break;
            }
        }

        return value < maxCoeff;
    }

    void clearWatches(IneqPropagator<T>& prop) {
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


    void swapWatch(IneqPropagator<T>& prop, size_t& i, size_t& j, bool& keepWatch, Lit& blockingLiteral, Lit& falsifiedLit, bool init) {
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

    void initWatch(IneqPropagator<T>& prop) {
        this->template fixWatch<true>(prop);
    }

    bool updateWatch(IneqPropagator<T>& prop, Lit falsifiedLit = Lit::Undef()) {
        return this->template fixWatch<false>(prop, falsifiedLit);
    }

    // returns if the watch is kept
    template<bool autoInit>
    bool fixWatch(IneqPropagator<T>& prop, Lit falsifiedLit = Lit::Undef()) {
        // bool isSat = false;
        bool keepWatch = true;

        if (header.isMarkedForDeletion) {
            keepWatch = false;
            return keepWatch;
        }
        // prop.visit += 1;

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

        const auto& value = prop.propMaster.getAssignment().value;

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
                    if (prop.propMaster.getPhase()[terms[k].lit] == State::True) {
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
            prop.propMaster.conflict();
            // prop.visit_required += 1;
        } else if (slack < maxCoeff) {
            for (size_t i = 0; i < this->watchSize; i++) {
                if (terms[i].coeff > slack
                    && value[terms[i].lit] == State::Unassigned)
                {
                    prop.propMaster.enqueue(
                        terms[i].lit,
                        std::make_unique<IneqReason<T>>(*this, prop));
                    // prop.visit_required += 1;
                }
            }
        }

        return keepWatch;
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
};

template<typename T>
inline std::ostream& operator<<(std::ostream& os, const FixedSizeInequality<T>& v) {
    for (const Term<T>& term: v.terms) {
        os << term << " ";
    };
    os << " >= " << v.degree;
    return os;
}

template<typename T>
inline std::ostream& operator<<(std::ostream& os, const IneqPropagator<T>& v) {
    os << "IneqPropagator State:\n";
    for (size_t i = 1; i < v.watchlist.size() / 2; ++i) {
        Var var(i);
        for (bool sign: {false, true}) {
            Lit lit1(var, sign);
            if (v.watchlist[lit1].size() == 0) {
                continue;
            }
            os << "\t" << lit1 << ":\n";
            for (size_t j = 0; j < v.watchlist[lit1].size(); ++j) {
                os << "\t\t" << *v.watchlist[lit1][j].ineq << "\n";
            }
        }
    }
    return os;
}

inline std::ostream& operator<<(std::ostream& os, const ClausePropagator& v) {
    os << "ClausePropagator State:\n";
    for (size_t i = 1; i < v.watchlist.size() / 2; ++i) {
        Var var(i);
        for (bool sign: {false, true}) {
            Lit lit1(var, sign);
            if (v.watchlist[lit1].size() == 0) {
                continue;
            }
            os << "\t" << lit1 << ":\n";
            for (size_t j = 0; j < v.watchlist[lit1].size(); ++j) {
                os << "\t\t" << *v.watchlist[lit1][j].ineq << "\n";
            }
        }
    }
    return os;
}

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

public:
    using TStoredConstraint = TConstraint;

    TConstraint* ineq = nullptr;
    size_t capacity = 0;

    void free(){
        if (ineq != nullptr) {
            ineq->~TConstraint();
            std::free(ineq);
            ineq = nullptr;
        }
    }

    ConstraintHandler(size_t size) {
        void* addr = malloc(size);
        ineq = new (addr) TConstraint(size);
    }

    ConstraintHandler(ConstraintHandler<TConstraint>&& other) noexcept {
        ineq = other.ineq;
        other.ineq = nullptr;
    }

    ConstraintHandler(const TConstraint& copyFrom) {
        void* addr = malloc(copyFrom.size());
        ineq = new (addr) TConstraint(copyFrom);
    }

    ConstraintHandler(const ConstraintHandler<TConstraint>& other):
        ConstraintHandler(*other.ineq) {}
    ConstraintHandler() {}

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
        return *this;
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

namespace InplaceIneqOps {
    struct saturate {
        /* returns true if the resulting constraint is normalized */
        template<typename TIneq>
        bool operator()(TIneq& ineq) {
            for (auto& term: ineq.terms) {
                using namespace std;
                term.coeff = min(term.coeff, ineq.degree);
            }
            return ineq.degree > 0 || ineq.terms.size() == 0;
        }

        template<>
        bool operator()(Clause& ineq) {
            return true;
        }
    };

    struct divide {
        template<typename TIneq, typename TInt>
        void operator()(TIneq& ineq, TInt divisor) {
            ineq.degree = divideAndRoundUp(ineq.degree, divisor);
            for (auto& term: ineq.terms) {
                term.coeff = divideAndRoundUp(term.coeff, divisor);
            }
        }

        template<typename TInt>
        void operator()(Clause& ineq, TInt divisor) {}
    };

    struct multiply {
        template<typename TIneq, typename TInt>
        void operator()(TIneq& ineq, TInt factor) {
            // static_assert(!std::is_same<TIneq, Clause>(), "Can not do inplace negation with clauses!");
            ineq.degree *= factor;
            for (auto& term: ineq.terms) {
                term.coeff *= factor;
            }
        }

        template<typename TInt>
        void operator()(Clause& ineq, TInt factor) {
            assert(false);
        }
    };

    struct negate {
        template<typename TIneq>
        void operator()(TIneq& ineq) {
            // static_assert(!std::is_same<TIneq, Clause>(), "Can not do inplace negation with clauses!");

            ineq.degree = -ineq.degree + 1;
            for (auto& term:ineq.terms) {
                ineq.degree += term.coeff;
                term.lit = ~term.lit;
            }
        }

        template<>
        void operator()(Clause& ineq) {
            assert(false);
        }
    };

    struct isContradiction {
        template<typename TIneq>
        bool operator()(TIneq& ineq) {
            using TInt = decltype(ineq.degree);
            TInt slack = -ineq.degree;
            for (auto& term: ineq.terms) {
                slack += term.coeff;
                if (slack > 0) {
                    return false;
                }
            }
            return true;
        }

        template<>
        bool operator()(Clause& ineq) {
            return ineq.terms.size() == 0;
        }
    };


    struct equals {
        template<typename TIneqA, typename TIneqB>
        bool operator()(TIneqA& ineqA, TIneqB& ineqB) {
            using TermA = typename TIneqA::TTerm;
            using TermB = typename TIneqB::TTerm;

            if ((ineqA.terms.size() != ineqB.terms.size())
                    || (ineqA.degree != ineqB.degree)) {
                return false;
            }

            std::vector<TermA> mine(ineqA.terms.begin(), ineqA.terms.end());
            sort(mine.begin(), mine.end(), orderByVar<TermA>);
            std::vector<TermB> theirs(ineqB.terms.begin(), ineqB.terms.end());
            sort(theirs.begin(), theirs.end(), orderByVar<TermB>);

            return std::equal(mine.begin(), mine.end(), theirs.begin());
        }
    };

    struct hash {
        template<typename TIneq>
        size_t operator()(TIneq& ineq) {
            using TTerm = typename TIneq::TTerm;
            std::vector<TTerm> mine(ineq.terms.begin(), ineq.terms.end());
            sort(mine.begin(), mine.end(), orderByVar<TTerm>);

            std::size_t seed = 0;

            for (const TTerm& term: mine) {
                hash_combine(seed, term.coeff);
                hash_combine(seed, term.lit);
            }

            hash_combine(seed, ineq.degree);
            return seed;
        }
    };

    struct implies {
        template<typename TIneqA, typename TIneqB>
        bool operator()(TIneqA& a, TIneqB& b) {
            // using IntTypeIterA = typename TIneqA::TTerm::coeff_type;
            // using IntTypeIterB = typename TIneqB::TTerm::coeff_type;

            // using Type1 = typename int_conversion<IntTypeIterA, IntTypeIterB>::stronger;
            // using Type2 = typename int_conversion<Type1, IntTypeA>::stronger;
            // using IntTypeStronger = typename int_conversion<Type2, IntTypeB>::stronger;
            // using IntSumSafe = typename int_information<IntTypeStronger>::sum_safe;

            using TTerm = typename TIneqB::TTerm;
            std::unordered_map<size_t, TTerm> lookup;
            for (auto& term: b.terms) {
                lookup.insert(std::make_pair(term.lit.var(), term));
            }

            // IntSumSafe weakenCost = 0;
            BigInt weakenCost = 0;
            for (auto& term: a.terms) {
                using namespace std;
                size_t var = term.lit.var();

                auto search = lookup.find(var);
                if (search == lookup.end()) {
                    weakenCost += term.coeff;
                } else {
                    auto theirs = search->second;
                    if (term.lit != theirs.lit) {
                        weakenCost += term.coeff;
                    } else if (term.coeff > theirs.coeff) {
                        if (theirs.coeff < b.degree) {
                            // only weaken if target coeffient is not saturated
                            weakenCost += term.coeff;
                            weakenCost -= theirs.coeff;
                        }
                    }
                }
            }
            weakenCost -= a.degree;
            weakenCost += b.degree;

            return weakenCost <= 0;
        }
    };

    struct substitute {
        template<typename TIneq>
        void operator()(TIneq& ineq, Substitution& sub) {
            // warning: after this operation the constraint is no longer
            // normalized, load to FatInequality to normalize
            for (auto& term: ineq.terms) {
                auto it = sub.map.find(term.lit);
                if (it != sub.map.end()) {
                    term.lit = it->second;
                }
            }
        }
    };

    struct print {
        template<typename TIneq, typename Stream>
        Stream& operator()(const TIneq& ineq, std::function<std::string(int)> varName, Stream& out) {
            for (auto &term: ineq.terms) {
                out << term.coeff << " ";
                if (term.lit.isNegated()) {
                    out << "~";
                }
                out << varName(term.lit.var()) << " ";
            }
            out << ">= " << ineq.degree;
            return out;
        }
    };

}

template<typename T>
class IneqPropagator: public Propagator {
public:
    typedef WatchInfo<FixedSizeInequality<T>> WatchedType;
    typedef std::vector<WatchedType> WatchList;

    PropagationMaster& propMaster;
    LitIndexedVec<WatchList> watchlist;

    IneqPropagator(PropagationMaster& _propMaster, size_t _nVars)
        : propMaster(_propMaster)
        , watchlist(2 * (_nVars + 1))
    {
        // for (auto& ws: watchlist) {
        //     ws.reserve(50);
        // }
    }


    void watch(Lit lit, WatchedType& w) {
        watchlist[lit].push_back(w);
    }

    virtual void increaseNumVarsTo(size_t _nVars){
        watchlist.resize(2 * (_nVars + 1));
    }

    virtual void propagate() {
        const auto& trail = propMaster.getTrail();
        // std::cout << "IneqPropagator: propagating from: " << qhead << std::endl;
        // std::cout << *this << std::endl;
        while (qhead < trail.size() and !propMaster.isConflicting()) {
            Lit falsifiedLit = ~trail[qhead];
            // std::cout << "propagating: " << trail[qhead] << std::endl;

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
            for (; next != end && !propMaster.isConflicting(); next++) {
                auto fetch = next + lookAhead;
                if (fetch < end) {
                    __builtin_prefetch(fetch->ineq);
                }
                assert(next->other != Lit::Undef() || propMaster.getAssignment().value[next->other] == State::Unassigned);

                bool keepWatch = true;
                if (propMaster.getAssignment().value[next->other] != State::True) {
                    keepWatch = next->ineq->updateWatch(*this, falsifiedLit);
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

            qhead += 1;
        }
    }

    virtual void cleanupWatches() {
        for (WatchList& wl: watchlist) {
            wl.erase(
                std::remove_if(
                    wl.begin(),
                    wl.end(),
                    [](auto& watch){
                        // assert(!watch.ineq->header.isMarkedForDeletion);
                        return watch.ineq->header.isMarkedForDeletion;
                    }
                ),
                wl.end()
            );
        }
    }
};

/*
 * Automatically resets the trail to its previous state after the
 * scope is left. Requires that only new things were put on the
 * trail, i.e., the trail did not grow shorter.
 */
class AutoReset {
private:
    PropagationMaster& engine;
    PropState base;
public:
    AutoReset(PropagationMaster& engine_)
        : engine(engine_)
        , base(engine.current) {

    }

    ~AutoReset(){
        engine.reset(base);
    }
};


template<typename T>
class PropEngine {
private:
    typedef std::unordered_set<Inequality<T>*>  OccursList;

    typedef Inequality<T> Ineq;
    std::unordered_set<Ineq*, PointedHash<Ineq>, PointedEq<Ineq>> constraintLookup;

    size_t nVars;
    bool updateWatch = true;
    size_t dbMem = 0;
    size_t cumDbMem = 0;
    size_t maxDbMem = 0;

    PropagationMaster propMaster;
    IneqPropagator<T> tmpPropagator;
    bool hasDetached = false;

public:
    IneqPropagator<T> ineqPropagator;
    ClausePropagator clausePropagator;

    LitIndexedVec<OccursList> occurs;
    std::vector<Inequality<T>*> unattached;
    std::vector<Inequality<T>*> propagatingAt0;
    std::chrono::duration<double> timeEffected;
    std::chrono::duration<double> timeFind;
    std::chrono::duration<double> timeInitProp;
    std::chrono::duration<double> timePropagate;
    std::chrono::duration<double> timeRUP;


    long long visit = 0;
    long long visit_sat = 0;
    long long visit_required = 0;
    long long lookup_requests = 0;


    PropEngine(size_t _nVars)
        : nVars(_nVars)
        , propMaster(_nVars)
        , tmpPropagator(propMaster, _nVars)
        , ineqPropagator(propMaster, _nVars)
        , clausePropagator(propMaster, _nVars)
        , occurs(2 * (_nVars + 1))
        , timeEffected(0)
        , timeFind(0)
        , timeInitProp(0)
        , timeRUP(0)
    {
        propMaster.propagators.push_back(&clausePropagator);
        propMaster.propagators.push_back(&ineqPropagator);
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

        std::cout << "c statistic: time cleanupTrail: "
            << std::fixed << std::setprecision(2)
            << propMaster.timeCleanupTrail.count() << std::endl ;

        std::cout << "c statistic: time propagate: "
            << std::fixed << std::setprecision(2)
            << timePropagate.count() << std::endl ;

        std::cout << "c statistic: time rup: "
            << std::fixed << std::setprecision(2)
            << timeRUP.count() << std::endl ;


        std::cout << "c statistic: hashColisions: " << hashColision << std::endl;
        std::cout << "c statistic: lookup_requests: " << lookup_requests << std::endl;
    }

    void increaseNumVarsTo(size_t _nVars){
        assert(nVars <= _nVars);
        if (nVars < _nVars) {
            this->nVars = _nVars;
            propMaster.increaseNumVarsTo(_nVars);
            occurs.resize(2 * (_nVars + 1));
        }
    }

    void propagate(){
        Timer timer(timePropagate);
        propMaster.propagate();
    }

    bool propagate4sat(std::vector<int>& lits) {
        AutoReset reset(this->propMaster);

        for (int lit: lits) {
            Lit l(lit);
            auto val = propMaster.getAssignment().value[l];

            if (val == State::Unassigned) {
                propMaster.enqueue(l, nullptr);
            } else if (val == State::False) {
                propMaster.conflict();
                break;
            }
        }

        propagate();

        bool success = false;
        if (!propMaster.isConflicting()) {
            success = true;
            for (uint var = 1; var <= nVars; var++) {
                auto val = propMaster.getAssignment().value[Lit(var)];
                if (val == State::Unassigned) {
                    success = false;
                    break;
                }
            }
        }

        return success;
    }

    bool checkSat(std::vector<int>& lits) {
        // AutoReset reset(this->propMaster);
        initPropagation();
        propagate();
        return propagate4sat(lits);
    }

    void _attach(Inequality<T>* ineq) {
        ineq->wasAttached = true;

        if (ineq->isPropagatingAt0()) {
            this->propagatingAt0.push_back(ineq);
        }

        ineq->initWatch(*this);
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

        if (hasDetached) {
            if (!propMaster.isTrailClean()) {
                propMaster.cleanupTrail();
                for (Inequality<T>* ineq: this->propagatingAt0) {
                    ineq->updateWatch(*this);
                }
            }
            // either cleanupWatches here or when detached, currently
            // watches should be cleaned while detached
            // propMaster.cleanupWatches();
            MasterJunkyard::get().clearAll();
            hasDetached = false;
        }

        attachUnattached();
        // std::cout << "after init:\n";
        // std::cout << clausePropagator;
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
                    if (ineq->isReason()) {
                        hasDetached = true;
                    }
                    if (ineq->isPropagatingAt0()) {
                        auto& propagating = propagatingAt0;
                        propagating.erase(
                            std::remove_if(
                                propagating.begin(),
                                propagating.end(),
                                [ineq](auto& other){
                                    return other == ineq;
                                }
                            ),
                            propagating.end()
                        );
                    }

                    ineq->clearWatches(*this);

                    ineq->markedForDeletion();
                }
            }
        }
    }

    std::vector<int> propagatedLits() {
        // AutoReset reset(this->propMaster);
        initPropagation();
        propagate();

        std::vector<int> assignment;
        for (uint var = 1; var <= nVars; var++) {
            State val = propMaster.getAssignment().value[Lit(var)];
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




    struct rupCheck {
        template<typename TIneq>
        bool operator()(const TIneq& redundant, PropEngine<T>& engine) {
            Timer timer(engine.timeRUP);
            engine.initPropagation();
            engine.propagate();

            FixedSizeInequalityHandler<T> negated(redundant.terms.size(), redundant.terms.size(), redundant.terms.begin(), redundant.terms.end(), redundant.degree);
            {
                AutoReset reset(engine.propMaster);
                InplaceIneqOps::negate()(*negated.ineq);

                engine.tmpPropagator.increaseNumVarsTo(engine.nVars);
                engine.propMaster.propagators.push_back(&engine.tmpPropagator);
                negated->initWatch(engine.tmpPropagator);

                engine.propagate();

                negated->clearWatches(engine.tmpPropagator);
                engine.propMaster.propagators.pop_back();
                // for (Lit lit : this->trail) {
                //     std::cout << lit << " ";
                // }
                // std::cout << std::endl;
                return engine.propMaster.isConflicting();
            }
        }
    };

    struct addOccurence {
        template<typename TIneq>
        void operator()(TIneq& ineq, PropEngine<T>& engine, Inequality<T>& w){
            for (auto& term: ineq.terms) {
                engine.occurs[term.lit].emplace(&w);
            }
        }
    };

    struct rmOccurence {
        template<typename TIneq>
        void operator()(TIneq& ineq, PropEngine<T>& engine, Inequality<T>& w){
            for (auto& term: ineq.terms) {
                engine.occurs[term.lit].erase(&w);
            }
        }
    };

    struct initWatch {
        void operator()(FixedSizeInequality<T>& constraint, PropEngine<T>& engine) {
            constraint.initWatch(engine.ineqPropagator);
        }

        void operator()(Clause& constraint, PropEngine<T>& engine) {
            constraint.initWatch(engine.clausePropagator);
        }
    };

    struct clearWatch {
        void operator()(FixedSizeInequality<T>& constraint, PropEngine<T>& engine) {
            constraint.clearWatches(engine.ineqPropagator);
        }

        void operator()(Clause& constraint, PropEngine<T>& engine) {
            constraint.clearWatches(engine.clausePropagator);
        }
    };

    struct updateWatches {
        void operator()(FixedSizeInequality<T>& constraint, PropEngine<T>& engine) {
            constraint.updateWatch(engine.ineqPropagator);
        }

        void operator()(Clause& constraint, PropEngine<T>& engine) {
            constraint.updateWatch(engine.clausePropagator);
        }
    };
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

    template<typename TConstraint>
    void load(TConstraint& ineq) {
        assert(!bussy && "critical error: I am used twice!");
        bussy = true;

        this->degree = ineq.degree;
        Term<T> myTerm;
        for (auto& term: ineq.terms) {
            if (term.coeff < 0) {
                myTerm.lit = ~term.lit;
                myTerm.coeff = term.coeff;
                myTerm.coeff = -myTerm.coeff;
                this->degree += term.coeff;
            } else {
                myTerm.lit = term.lit;
                myTerm.coeff = std::move(term.coeff);
            }
            addLhs(myTerm);
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
            T& coeff = this->coeffs[var];
            Lit lit(var, coeff < 0);
            using namespace std;
            coeff = abs(coeff);

            if (coeff > 0) {
                assert(var != Substitution::one().var());
                assert(pos != ineq.terms.end());
                pos->coeff = std::move(coeff);
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
    template<typename IntType>
    void addLhs(const Term<IntType> &term) {
        using namespace std;
        T b = cpsign(term.coeff, term.lit);
        Var var = term.lit.var();
        this->use(var);
        T a = this->coeffs[var];
        if (a == 0) {
            this->coeffs[var] = b;
        } else {
            this->coeffs[var] = a + b;
            T cancellation = max<T>(0, max(abs(a), abs(b)) - abs(this->coeffs[var]));
            this->degree -= cancellation;
        }
    }

    template<typename TConstraint>
    void add(const TConstraint& other) {
        for (const auto &term:other.terms) {
            addLhs(term);
        }

        this->degree += other.degree;
    }

    struct callLoad {
        template<typename TConstraint, typename Calee>
        void operator()(TConstraint& constraint, Calee& callee){
            callee.load(constraint);
        }
    };

    struct callAdd {
        template<typename TConstraint>
        void operator()(TConstraint& constraint, FatInequality<T>& callee){
            callee.add(constraint);
        }
    };
};

using IneqLarge = FixedSizeInequality<CoefType>;
using IneqBig = FixedSizeInequality<BigInt>;

enum class TypeId {
    Clause, IneqLarge, IneqBig
};

template<typename T>
struct type_id;

template<>
struct type_id<Clause> {
    static constexpr TypeId value = TypeId::Clause;
};

template<>
struct type_id<IneqLarge> {
    static constexpr TypeId value = TypeId::IneqLarge;
};

template<>
struct type_id<IneqBig> {
    static constexpr TypeId value = TypeId::IneqBig;
};

class BaseHandle;
using HandlePtr = std::unique_ptr<BaseHandle>;

class BaseHandle {
public:
    const TypeId typeId;
    BaseHandle(TypeId _typeId)
        : typeId(_typeId)
    {}
    virtual HandlePtr copy() = 0;
    virtual void toJunkyard() = 0;
    virtual bool isReason() = 0;
    virtual bool isPropagatingAt0() = 0;
    virtual void markedForDeletion() = 0;
    virtual size_t mem() = 0;
    virtual ~BaseHandle() = default;
};

template<typename TConstraint>
struct Handle : public BaseHandle {
    ConstraintHandler<TConstraint> manager;

    Handle(ConstraintHandler<TConstraint>&& _manager)
        : BaseHandle(type_id<TConstraint>::value)
        , manager(std::move(_manager))
    {}

    virtual HandlePtr copy() {
        return std::make_unique<Handle<TConstraint>>(ConstraintHandler<TConstraint>(manager));
    };

    TConstraint& get() {
        assert(manager.ineq != nullptr);
        return *manager.ineq;
    }

    virtual void toJunkyard() {
        Junkyard<ConstraintHandler<TConstraint>>::get().add(std::move(manager));
    }

    virtual bool isReason() {
        return get().header.isReason;
    }

    virtual void markedForDeletion() {
        get().header.isMarkedForDeletion = true;
    }

    virtual bool isPropagatingAt0() {
        return get().isPropagatingAt0();
    }

    virtual size_t mem() {
        return sizeof(TConstraint) + get().terms.size() * sizeof(typename TConstraint::TTerm);
    }

    virtual ~Handle() = default;
};

template<typename TConstraintHandler>
static HandlePtr make_handle(TConstraintHandler&& handler) {
    using TConstraint = typename TConstraintHandler::TStoredConstraint;
    return HandlePtr(new Handle<TConstraint>(std::move(handler)));
}

constexpr int8_t combineTypeId(TypeId idA, TypeId idB) {
    return static_cast<int8_t>(idA) << 4 | static_cast<int8_t>(idB);
};


template<typename used>
struct unpacked;

template<>
struct unpacked<CoefType> {
    template<typename method, typename ...Args>
    static auto call(method m, BaseHandle* a, Args && ...args) {
        // we could totaly use virtual function calls here, but as we need
        // the infrastructure anyway to call methods on pairs of
        // inequalities, why not use it allways and save(?) some typing.
        switch (a->typeId) {
            case TypeId::Clause:
                return m(static_cast<Handle<Clause>*>(a)->get(), std::forward<Args>(args)...);
                break;
            case TypeId::IneqLarge:
                return m(static_cast<Handle<IneqLarge>*>(a)->get(), std::forward<Args>(args)...);
                break;
            default:
                assert(false);
                throw "Cast error.";
        }
    }

    struct doubleUnpack {
        template<typename UnpackedB, typename method, typename ...Args>
        auto operator()(UnpackedB& b, BaseHandle* a, method m, Args && ...args){
            return call(m, a, b, std::forward<Args>(args)...);
        }
    };

    template<typename method, typename ...Args>
    static auto call2(method m, BaseHandle* a, BaseHandle* b, Args && ...args) {
        return call(doubleUnpack(), b, a, m, std::forward<Args>(args)...);
    }
};

template<>
struct unpacked<BigInt> {
    template<typename method, typename ...Args>
    static auto call(method m, BaseHandle* a, Args && ...args) {
        // we could totaly use virtual function calls here, but as we need
        // the infrastructure anyway to call methods on pairs of
        // inequalities, why not use it allways and save(?) some typing.
        switch (a->typeId) {
            case TypeId::Clause:
                return m(static_cast<Handle<Clause>*>(a)->get(), std::forward<Args>(args)...);
                break;
            case TypeId::IneqBig:
                return m(static_cast<Handle<IneqBig>*>(a)->get(), std::forward<Args>(args)...);
                break;
            default:
                assert(false);
                throw "Cast error.";
        }
    }

    struct doubleUnpack {
        template<typename UnpackedB, typename method, typename ...Args>
        auto operator()(UnpackedB& b, BaseHandle* a, method m, Args && ...args){
            return call(m, a, b, std::forward<Args>(args)...);
        }
    };

    template<typename method, typename ...Args>
    static auto call2(method m, BaseHandle* a, BaseHandle* b, Args && ...args) {
        return call(doubleUnpack(), b, a, m, std::forward<Args>(args)...);
    }
};




// template<typename method>
// auto callUnpacked(method m, BaseHandle* a, BaseHandle* b) {
//     int8_t test = combineTypeId(a->typeId, b->typeId);
//     switch (test) {
//         case combineTypeId(TypeId::Clause, TypeId::Clause):
//             return m(static_cast<Handle<Clause>*>(a)->get(), static_cast<Handle<Clause>*>(b)->get());
//             break;
//         case combineTypeId(TypeId::Clause, TypeId::IneqLarge):
//             return m(static_cast<Handle<Clause>*>(a)->get(), static_cast<Handle<IneqLarge>*>(b)->get());
//             break;
//         case combineTypeId(TypeId::IneqLarge, TypeId::Clause):
//             return m(static_cast<Handle<IneqLarge>*>(a)->get(), static_cast<Handle<Clause>*>(b)->get());
//             break;
//         case combineTypeId(TypeId::IneqLarge, TypeId::IneqLarge):
//             return m(static_cast<Handle<IneqLarge>*>(a)->get(), static_cast<Handle<IneqLarge>*>(b)->get());
//             break;
//         default:
//             assert(false);
//             throw "unreachable";
//     }
// }




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

    HandlePtr handle;

    static std::vector<FatInequalityPtr<T>> pool;
    const bool useClauses = true;

    friend std::hash<Inequality<T>>;
public:
    bool isAttached = false;
    bool wasAttached = false;
    uint64_t minId = std::numeric_limits<uint64_t>::max();
    std::unordered_set<uint64_t> ids;
    uint detachCount = 0;

    Inequality(Inequality& other)
        : handle(other.handle->copy())
    {
        assert(other.loaded == false);
        this->minId = other.minId;
    }

    Inequality(FixedSizeInequalityHandler<T>&& handler)
        : handle(make_handle(std::move(handler)))
    {
        expand();
        contract();
    }


    Inequality(std::vector<Term<T>>& terms_, T degree_)
        : Inequality(FixedSizeInequalityHandler<T>(terms_.size(), terms_, degree_))
    {}

    Inequality(std::vector<Term<T>>&& terms_, T degree_)
        : Inequality(FixedSizeInequalityHandler<T>(terms_.size(), terms_, degree_))
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

        if (wasAttached) {
            assert(handle);
            handle->toJunkyard();
        }
    }

    bool isReason() {
        return handle->isReason();
    }

    void freeze(size_t numVars) {
        contract();
        frozen = true;
    }

    void clearWatches(PropEngine<T>& prop) {
        assert(frozen);
        unpacked<T>::call(typename PropEngine<T>::clearWatch(), handle.get(), prop);
    }

    void markedForDeletion() {
        assert(frozen);
        handle->markedForDeletion();
    }

    bool isPropagatingAt0() {
        return handle->isPropagatingAt0();
    }

    void initWatch(PropEngine<T>& prop) {
        assert(frozen && "Call freeze() first.");
        unpacked<T>::call(typename PropEngine<T>::initWatch(), handle.get(), prop);
    }

    void updateWatch(PropEngine<T>& prop) {
        assert(frozen && "Call freeze() first.");
        unpacked<T>::call(typename PropEngine<T>::updateWatches(), handle.get(), prop);
    }

    Inequality* saturate(){
        assert(!frozen);
        contract();

        bool isNormalized = unpacked<T>::call(InplaceIneqOps::saturate(), handle.get());

        if (!isNormalized) {
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
        unpacked<T>::call(InplaceIneqOps::divide(), handle.get(), divisor);
        return this;
    }

    Inequality* multiply(T factor){
        assert(!frozen);
        contract();
        unpacked<T>::call(InplaceIneqOps::multiply(), handle.get(), factor);
        return this;
    }

    Inequality* add(Inequality* other){
        assert(!frozen);
        expand();
        other->contract();
        unpacked<T>::call(typename FatInequality<T>::callAdd(), handle.get(), *expanded);
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

            unpacked<T>::call(typename FatInequality<T>::callLoad(), handle.get(), *expanded);
        }
    }

    void contract() {
        if (loaded) {
            assert(!frozen);
            FixedSizeInequalityHandler<T> manager(expanded->size());
            expanded->unload(*manager.ineq);
            handle = HandlePtr(new Handle<FixedSizeInequality<T>>(std::move(manager)));
            pool.push_back(std::move(expanded));
            expanded = nullptr;
            loaded = false;
        }
    }

    bool eq(Inequality* other) {
        return *this == *other;
    }

    bool operator==(Inequality<T>& other) {
        contract();
        other.contract();
        return unpacked<T>::call2(InplaceIneqOps::equals(), handle.get(), other.handle.get());
    }

    bool operator==(const Inequality<T>& other) const {
        assert(!this->loaded);
        assert(!other.loaded);
        return unpacked<T>::call2(InplaceIneqOps::equals(), handle.get(), other.handle.get());
    }

    bool operator!=(Inequality<T>& other) {
        return !(*this == other);
    }

    std::string toString(std::function<std::string(int)> varName) {
        contract();
        std::stringstream s;
        // unpacked<T>::call(InplaceIneqOps::print(), handle.get(), varName, s);
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

        return unpacked<T>::call2(InplaceIneqOps::implies(), handle.get(), other->handle.get());
    }

    bool rupCheck(PropEngine<T>& propEngine) {
        this->contract();
        return unpacked<T>::call(typename PropEngine<T>::rupCheck(), handle.get(), propEngine);
    }

    Inequality* substitute(Substitution& sub) {
        assert(!this->frozen);
        this->contract();
        unpacked<T>::call(InplaceIneqOps::substitute(), handle.get(), sub);
        // need to normalize, we do so by expanding the constraint
        this->expand();
        return this;
    }

    Inequality* negated() {
        assert(!frozen);
        this->contract();
        unpacked<T>::call(InplaceIneqOps::negate(), handle.get());
        return this;
    }

    Inequality* copy(){
        contract();
        return new Inequality(*this);
    }

    Inequality* weaken(int _var) {
        assert(!frozen);
        assert(_var >= 0);
        Var var(_var);
        expand();
        expanded->weaken(var);
        return this;
    }

    bool isContradiction(){
        contract();
        return unpacked<T>::call(InplaceIneqOps::isContradiction(), handle.get());
    }

    size_t mem() {
        contract();
        return handle->mem();
    }

    void registerOccurence(PropEngine<T>& prop) {
        assert(frozen);
        return unpacked<T>::call(typename PropEngine<T>::addOccurence(), handle.get(), prop, *this);
    }

    void unRegisterOccurence(PropEngine<T>& prop) {
        assert(frozen);
        return unpacked<T>::call(typename PropEngine<T>::rmOccurence(), handle.get(), prop, *this);
    }
};

namespace std {
    template <typename T>
    struct hash<Inequality<T>> {
        std::size_t operator()(const Inequality<T>& ineq) const {
            return unpacked<T>::call(InplaceIneqOps::hash(), ineq.handle.get());
        }
    };
}

// we need to initialzie the static template member manually;
template<typename T>
std::vector<FatInequalityPtr<T>> Inequality<T>::pool;
