#include <cstdint>
#include <vector>
#include <cassert>
#include <iostream>
#include <iterator>

#include <unordered_map>

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

template<typename IntType>
class Term {
    IntType coeff;
    Lit lit;
public:
    using coeff_type = IntType;

    IntType& getCoeff() {
        return coeff;
    }

    const IntType& getCoeff() const {
        return coeff;
    }

    const Lit& getLit() const {
        return lit;
    }

    Lit& getLit(){
        return lit;
    }

    template<typename T>
    Term(T _coeff, Lit _lit)
        : coeff(_coeff)
        , lit(_lit)
    {}

    template<typename T>
    Term(const Term<T>& other)
        : Term(other.getCoeff(), other.getLit())
    {}
};

template<>
class Term<void> {
    Lit lit;

public:
    using coeff_type = int8_t;

    const int8_t& getCoeff() const {
        static const int8_t one = 1;
        return one;
    }

    const Lit& getLit() const {
        return lit;
    }

    Lit& getLit() {
        return lit;
    }

    template<typename T>
    Term(T _coeff, Lit _lit)
        : lit(_lit)
    {
        assert(_coeff == 1);
    }

    template<typename T>
    Term(const Term<T>& other)
        : Term(other.getCoeff(), other.getLit())
    {}
};

template<typename FromIter, typename ToType>
class TermIter {
    FromIter iter;
public:
    TermIter(FromIter _iter)
        : iter(_iter)
    {}

    Term<ToType> operator*(){
        return Term<ToType>(*iter);
    }

    bool operator==(TermIter<FromIter,ToType>& other){
        return this->iter == other.iter;
    }

    bool operator!=(TermIter<FromIter,ToType>& other){
        return this->iter != other.iter;
    }

    void operator++(){
        ++iter;
    }
};

template<typename a>
struct int_information;

template<>
struct int_information<int8_t> {
    using sum_safe = int32_t;
};

template<>
struct int_information<int32_t> {
    using sum_safe = int64_t;
};

template<typename a, typename b>
struct int_conversion;

template<>
struct int_conversion<int8_t, int8_t> {
    using stronger = int8_t;
    using weaker = int8_t;
};

template<>
struct int_conversion<int8_t, int32_t> {
    using stronger = int32_t;
    using weaker = int8_t;
};

template<>
struct int_conversion<int32_t, int32_t> {
    using stronger = int32_t;
    using weaker = int32_t;
};

template<>
struct int_conversion<int32_t, int8_t> {
    using stronger = int32_t;
    using weaker = int8_t;
};



struct CheckAImpliesB {
template<typename IterA, typename IntTypeA, typename IterB, typename IntTypeB>
    bool aImpliesB(IterA itA, IterA endA, IntTypeA degreeA, IterB itB, IterB endB, IntTypeB degreeB) {
        using IntTypeIterA = typename std::iterator_traits<IterA>::value_type::coeff_type;
        using IntTypeIterB = typename std::iterator_traits<IterB>::value_type::coeff_type;

        using Type1 = typename int_conversion<IntTypeIterA, IntTypeIterB>::stronger;
        using Type2 = typename int_conversion<Type1, IntTypeA>::stronger;
        using IntTypeStronger = typename int_conversion<Type2, IntTypeB>::stronger;
        using IntSumSafe = typename int_information<IntTypeStronger>::sum_safe;

        std::unordered_map<size_t, Term<IntTypeIterB>> lookup;
        for (; itB != endB ; ++itB) {
            lookup.insert(std::make_pair(itB->getLit().var(), *itB));
        }

        IntSumSafe weakenCost = 0;
        for (; itA != endA; ++itA) {
            using namespace std;
            size_t var = itA->getLit().var();

            auto search = lookup.find(var);
            if (search == lookup.end()) {
                weakenCost += itA->getCoeff();
            } else {
                auto theirs = search->second;
                if (itA->getLit() != theirs.getLit()) {
                    weakenCost += itA->getCoeff();
                } else if (itA->getCoeff() > theirs.getCoeff()) {
                    if (theirs.getCoeff() < degreeB) {
                        // only weaken if target getcoefficient is not saturated
                        weakenCost += itA->getCoeff();
                        weakenCost -= theirs.getCoeff();
                    }
                }
            }
        }
        weakenCost -= degreeA;
        weakenCost += degreeB;

        return weakenCost <= 0;
    }

    template<typename TypeA, typename TypeB>
    bool operator()(TypeA a, TypeB b) {
        return aImpliesB(
                a.terms.begin(), a.terms.end(), a.getDegree(),
                b.terms.begin(), b.terms.end(), b.getDegree()
            );
    }
};

template<typename ToType, typename FromIter>
TermIter<FromIter, ToType> make_iter(FromIter iter) {
    return TermIter<FromIter, ToType>(iter);
}



enum class TypeId {
    Clause, Ineq32
};

class BaseIneq {
public:
    virtual TypeId getTypeId() = 0;
};

class Clause: public BaseIneq {
public:
    std::vector<Term<void>> terms;

    int8_t getDegree() {
        return 1;
    }

    virtual TypeId getTypeId(){
        return TypeId::Clause;
    }
};

class Ineq32: public BaseIneq {
public:
    std::vector<Term<int32_t>> terms;
    int32_t degree;

    int32_t getDegree() {
        return degree;
    }

    virtual TypeId getTypeId(){
        return TypeId::Ineq32;
    }
};

constexpr int8_t combineTypeId(TypeId idA, TypeId idB) {
    return static_cast<int8_t>(idA) << 4 | static_cast<int8_t>(idB);
};

template<typename method>
auto callUnpacked(method m, BaseIneq* a, BaseIneq* b) {
    int8_t test = combineTypeId(a->getTypeId(), b->getTypeId());
    switch (test) {
        case combineTypeId(TypeId::Clause, TypeId::Clause):
            return m(*static_cast<Clause*>(a), *static_cast<Clause*>(b));
            break;
        case combineTypeId(TypeId::Clause, TypeId::Ineq32):
            return m(*static_cast<Clause*>(a), *static_cast<Ineq32*>(b));
            break;
        case combineTypeId(TypeId::Ineq32, TypeId::Clause):
            return m(*static_cast<Ineq32*>(a), *static_cast<Clause*>(b));
            break;
        case combineTypeId(TypeId::Ineq32, TypeId::Ineq32):
            return m(*static_cast<Ineq32*>(a), *static_cast<Ineq32*>(b));
            break;
        default:
            assert(false);
    }
}

class Inequality {
public:
    BaseIneq* store;
    Inequality(BaseIneq* store)
        : store(store)
    {}

    bool implies(Inequality* other) {
        return callUnpacked(CheckAImpliesB(), this->store, other->store);
    }
};

int main(int argc, char const *argv[])
{

    Clause clause;
    clause.terms.emplace_back(1,Lit(1));
    clause.terms.emplace_back(1,Lit(2));
    Inequality a(&clause);

    Ineq32 ineq32;
    ineq32.degree = 2;
    ineq32.terms.emplace_back(1,Lit(1));
    ineq32.terms.emplace_back(2,Lit(2));
    Inequality b(&ineq32);

    if (a.implies(&b)) {
        std::cout << "a implies b" << std::endl;
    } else {
        std::cout << "b implies a" << std::endl;
    }

    // auto it  = make_iter<int64_t>(v.begin());
    // auto end = make_iter<int64_t>(v.end());
    // for (;it!=end;++it) {
    //     Term<int64_t> term = *it;
    //     std::cout << term.getCoeff() << " " << term.getLit() << std::endl;
    // }

    return 0;
}