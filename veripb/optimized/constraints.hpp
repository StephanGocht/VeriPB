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
};


class Reason {
public:
    virtual void rePropagate() = 0;
    virtual bool isMarkedForDeletion() = 0;
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

class Clause;

using ClauseReason = GenericDBReason<Clause, ClausePropagator>;

class Clause {
private:
    using TElement = Lit;
    using TVec = InlineVec<TElement>;

    friend ClauseHandler;
    friend std::ostream& operator<<(std::ostream& os, const Clause& v);

public:
    DBConstraintHeader header;
    size_t propagationSearchStart = 2;
    InlineVec<Lit> literals;

private:
    // size_t size() const {
    //     return literals.size();
    // }

    Clause(size_t size)
        :literals(size) {
    }

    Clause(const Clause& other)
        : literals(other.literals.size())
    {
        using namespace std;
        copy(other.literals.begin(), other.literals.end(), this->literals.begin());
    }

    Clause(std::vector<Lit>& _literals)
        : literals(_literals.size())
    {
        std::copy(_literals.begin(),_literals.end(), this->literals.begin());
    }

    template<typename T>
    Clause(const FixedSizeInequality<T>& other)
        : literals(other.terms.size())
    {
        for (size_t i = 0; i < other.terms.size(); ++i) {
            literals[i] = other.terms[i].lit;
        }
    }

    void checkPosition(size_t pos, int &numFound, size_t (&watcher)[2], const Assignment& assignment) {
        Lit& lit = this->literals[pos];
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
        size_t nWatcher = std::min((size_t) 2, literals.size());
        for (size_t i = 0; i < nWatcher; i++) {
            auto& ws = prop.watchlist[literals[i]];
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
        for (Lit lit: literals) {
            assert(static_cast<size_t>(lit.var()) < prop.propMaster.getAssignment().value.size());
            assert(static_cast<size_t>(lit) < prop.watchlist.size());
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
                || (this->literals.size() < 2
                    || (this->literals[0] == falsifiedLit
                        || this->literals[1] == falsifiedLit
            )));


        int numFound = 0;
        // watcher will contain the position of the literals to be watched
        size_t watcher[2];

        const Assignment& assignment = prop.propMaster.getAssignment();

        if (this->literals.size() >= 2) {
            if (this->literals[1] == falsifiedLit) {
                std::swap(this->literals[0], this->literals[1]);
            }

            checkPosition(0, numFound, watcher, assignment);
            if (numFound < 2) {
                checkPosition(1, numFound, watcher, assignment);
            }

            size_t pos = this->propagationSearchStart;
            for (size_t i = 0; i < this->literals.size() - 2 && numFound < 2; i++) {
                checkPosition(pos, numFound, watcher, assignment);
                pos += 1;
                if (pos == this->literals.size()) {
                    pos = 2;
                }
            }
            this->propagationSearchStart = pos;
        } else if (this->literals.size() == 1) {
            checkPosition(0, numFound, watcher, assignment);
        }

        // LOG(debug) << "numFound: "<< numFound << EOM;
        if (numFound == 1) {
            Lit lit = this->literals[watcher[0]];
            if (assignment[lit] == State::Unassigned) {
                prop.propMaster.enqueue(
                    lit,
                    std::make_unique<ClauseReason>(*this, prop));
            }
        }

        if (numFound == 0) {
            prop.propMaster.conflict();
        }

        if (this->literals.size() >= 2) {
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
                    assert(watcher[i] < literals.size());

                    if (initial
                        || (i == 0
                            && watcher[i] >= 2
                            && falsifiedLit != Lit::Undef())) {
                        // was not a watched literal / needs to be readded
                        keep = false;
                        // assert(watcher[0] < watcher[1] || watcher[1] == 1);
                        assert(initial || (literals[i] == falsifiedLit));
                        std::swap(literals[i], literals[watcher[i]]);
                        WatchInfo<Clause> watchInfo;
                        watchInfo.ineq = this;
                        watchInfo.other = literals[watcher[(i + 1) % 2]];
                        prop.watch(literals[i], watchInfo);
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
    Term<int> term;
    term.coeff = 1;
    for (Lit lit: v.literals) {
        term.lit = lit;
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

public:
    DBConstraintHeader header;

    T degree;
    T maxCoeff = 0;

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

    FixedSizeInequality(Clause& clause)
        : degree(1)
        , terms(clause.literals.size())
    {
        Term<T> term;
        term.coeff = 1;
        Term<T>* it = this->terms.begin();
        for (Lit lit: clause) {
            term.lit = lit;
            *it = term;
            ++it;
        }
    }

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


    long long visit = 0;
    long long visit_sat = 0;
    long long visit_required = 0;
    long long lookup_requests = 0;


    PropEngine(size_t _nVars)
        : nVars(_nVars)
        , propMaster(_nVars)
        , ineqPropagator(propMaster, _nVars)
        , clausePropagator(propMaster, _nVars)
        , occurs(2 * (_nVars + 1))
        , timeEffected(0)
        , timeFind(0)
        , timeInitProp(0)
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

        std::cout << "c statistic: time propagate: "
            << std::fixed << std::setprecision(2)
            << timePropagate.count() << std::endl ;


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
                    hasDetached = true;
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

    bool rupCheck(const FixedSizeInequality<T>& redundant) {
        initPropagation();
        propagate();

        {
            AutoReset reset(this->propMaster);
            FixedSizeInequalityHandler<T> negated(redundant);
            negated->negate();

            IneqPropagator<T> tmpPropagator(propMaster, nVars);
            propMaster.propagators.push_back(&tmpPropagator);
            negated->initWatch(tmpPropagator);

            propagate();

            propMaster.propagators.pop_back();
            // for (Lit lit : this->trail) {
            //     std::cout << lit << " ";
            // }
            // std::cout << std::endl;
            return propMaster.isConflicting();
        }
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

    void addOccurence(Lit lit, Inequality<T>& w) {
        occurs[lit].emplace(&w);
    }

    void rmOccurence(Lit lit, Inequality<T>& w) {
        occurs[lit].erase(&w);
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
    void addLhs(const Term<T> &term) {
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
    ClauseHandler clauseHandler;

    static std::vector<FatInequalityPtr<T>> pool;

    friend std::hash<Inequality<T>>;
public:
    bool isAttached = false;
    bool wasAttached = false;
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

        if (wasAttached) {

            if (clauseHandler.ineq) {
                // assert(clauseHandler.ineq->header.isMarkedForDeletion);
                Junkyard<ClauseHandler>::get().add(std::move(clauseHandler));
            }
            // assert(ineq->header.isMarkedForDeletion);
            Junkyard<FixedSizeInequalityHandler<T>>::get().add(std::move(handler));
        }
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
        if (clauseHandler.ineq) {
            clauseHandler.ineq->clearWatches(prop.clausePropagator);
        } else {
            ineq->clearWatches(prop.ineqPropagator);
        }
    }

    void markedForDeletion() {
        if (ineq->degree == 1 && clauseHandler.ineq) {
            clauseHandler.ineq->header.isMarkedForDeletion = true;
        }
        ineq->header.isMarkedForDeletion = true;
    }

    bool isPropagatingAt0() {
        return ineq->isPropagatingAt0();
    }

    void initWatch(PropEngine<T>& prop) {
        assert(frozen && "Call freeze() first.");

        if (ineq->degree == 1) {
            assert(!clauseHandler.ineq);
            clauseHandler = ClauseHandler(ineq->terms.size(), *ineq);
            clauseHandler.ineq->initWatch(prop.clausePropagator);
        } else {
            ineq->initWatch(prop.ineqPropagator);
        }
    }

    void updateWatch(PropEngine<T>& prop) {
        assert(frozen && "Call freeze() first.");

        if (ineq->degree == 1) {
            assert(clauseHandler.ineq);
            clauseHandler.ineq->updateWatch(prop.clausePropagator);
        } else {
            ineq->updateWatch(prop.ineqPropagator);
        }
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
