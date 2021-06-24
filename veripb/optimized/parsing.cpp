#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <cctype>

#include "constraints.hpp"
#include "BigInt.hpp"

#ifdef PY_BINDINGS
    #include <pybind11/pybind11.h>
    #include <pybind11/stl.h>
    #include <pybind11/iostream.h>
    #include <pybind11/functional.h>
    namespace py = pybind11;
#endif


using std::string_view;

struct FileInfo {
    FileInfo(std::string _fileName)
        : fileName(_fileName)
    {}

    std::string fileName;
    size_t line = 0;
};

class WordIter;

class ParseError: std::runtime_error {
public:
    std::string fileName = "";
    size_t line = 0;
    size_t column = 0;

    explicit ParseError(const WordIter& it, const std::string& what_arg);

    mutable std::string msg;
    virtual char const * what() const noexcept override {
        std::stringstream what;
        if (fileName == "") {
            what << "?";
        } else {
            what << fileName;
        }
        what << ":";
        if (line == 0) {
            what << "?";
        } else {
            what << line;
        }
        what << ":";
        if (column == 0) {
            what << "?";
        } else {
            what << column;
        }
        what << ": " << runtime_error::what();
        msg = what.str();
        return msg.c_str();
    }
};

class WordIter {
public:
    const char* wordSeperator = " \t";

    FileInfo fileInfo;
    std::string line;
    string_view word;
    size_t start = 0;
    size_t endPos = 0;
    size_t nextStart = 0;
    bool isEndIt = false;

    void next() {
        start = nextStart;
        if (start != std::string::npos) {
            endPos = line.find_first_of(wordSeperator, start);
            if (endPos == std::string::npos) {
                nextStart = endPos;
                endPos = line.size();
            } else {
                nextStart = line.find_first_not_of(wordSeperator, endPos);
            }

            word = string_view(&line.data()[start], endPos - start);
        } else {
            word = "";
        }
    }

private:
    WordIter():fileInfo("WordIter::EndItterator"){isEndIt = true;}

public:
    static WordIter end;

    static std::istream& getline(std::istream& stream, WordIter& it) {
        it.fileInfo.line += 1;
        std::istream& result = std::getline(stream, it.line);
        if (!it.line.empty() && it.line.back() == '\r') {
            it.line.pop_back();
        }
        it.init();
        return result;
    }

    WordIter(std::string fileName)
        : fileInfo(fileName) {
    }

    WordIter(FileInfo _info)
        : fileInfo(_info)
    {}

    void init() {
        endPos = 0;
        nextStart = line.find_first_not_of(wordSeperator, endPos);
        next();
    }

    void expect(std::string word) {
        std::stringstream s;

        if (*this == WordIter::end) {
            s << "Expected '" << word << "'.";
            throw ParseError(*this, s.str());
        } else if (this->get() != word) {
            s << "Expected '" << word << "', but found '" << this->get() << "'.";
            throw ParseError(*this, s.str());
        }
    }

    void expectOneOf(std::vector<std::string> words) {
        std::stringstream s;
        s << "Expected one of ";
        bool first = true;
        for (const std::string& word: words) {
            if (first) {
                first = false;
            } else {
                s << ", ";
            }
            s << "'" << word << "'";
        }

        if (*this == WordIter::end) {
            s << "'.";
            throw ParseError(*this, s.str());
        }
        bool found = false;
        for (const std::string& word: words) {
            if (this->get() == word) {
                found = true;
                break;
            }
        }
        if (!found) {
            s << ", but found '" << this->get() << "'.";
            throw ParseError(*this, s.str());
        }
    }

    const string_view& get(){
        return word;
    }
    const string_view& operator*() const {return word;};
    const string_view* operator->() const {return &word;};

    WordIter& operator++() {
        next();
        return *this;
    }

    bool isEnd() const {
        return start == string_view::npos;
    }

    friend bool operator==(const WordIter& a, const WordIter& b) {
        if (a.isEndIt) {
            return (b.start == string_view::npos);
        } else if (b.isEndIt) {
            return (a.start == string_view::npos);
        } else {
            return (a.line == b.line) and (a.start == b.start) and (a.endPos == b.endPos);
        }

    }
    friend bool operator!=(const WordIter& a, const WordIter& b) {
        return !(a==b);
    }

    size_t getColumn() const {
        size_t column;
        if (this->start == std::string::npos) {
            column = this->line.size() + 1;
        } else {
            column = this->start + 1;
        }
        return column;
    }

    size_t getLine() const {
        return this->fileInfo.line;
    }

    std::string getLineText() const {
        return this->line;
    }

    std::string getFileName() const {
        return this->fileInfo.fileName;
    }



};

bool nextLine(std::ifstream* stream, WordIter* it) {
    return !!WordIter::getline(*stream, *it);
}

WordIter WordIter::end;

ParseError::ParseError(const WordIter& it, const std::string& what_arg)
    : std::runtime_error(what_arg)
{
    fileName = it.getFileName();
    line = it.getLine();
    column = it.getColumn();
}

/**
 * throws std::invalid_argument or std::out_of_range exception on
 * invalid input
 */
template<typename T>
T parseCoeff(const WordIter& it){
    // std::cout  << it->size() << std::endl;
    return parseCoeff<T>(it, 0, it->size());
};

template<typename T>
T parseCoeff(const WordIter& it, size_t start, size_t length);

int parseInt(const WordIter& word, std::string msg, size_t start, size_t length) {
    assert(word->size() >= start + length);
    assert(length > 0);

    if (word.isEnd()) {
        throw ParseError(word, "Expected Number.");
    }

    const char* it = word->data() + start;
    const char* end = word->data() + start + length;

    bool negated = false;
    if (*it == '+') {
        it += 1;
    } else if (*it == '-') {
        negated = true;
        it += 1;
    }

    if (it == end) {
        throw ParseError(word, "Expected Number, only got sign.");
    } else if (end - it > 9) {
        throw ParseError(word, "Number too large.");
    }

    int res = 0;
    for (; it != end; ++it) {
        uint8_t chr = *it - '0';
        res *= 10;
        res += chr;
        if (chr > 9) {
            throw ParseError(word, "Expected number.");
        }
    }
    if (negated) {
        res = -res;
    }

    return res;
    // try {
    //     return std::stoi(it->substr(start, length));
    // } catch (const std::invalid_argument& e) {
    //     throw ParseError(it, "Expected coefficient, did not get an integer.");
    // } catch (const std::out_of_range& e) {
    //     throw ParseError(it, "Coefficient too large.");
    // }
};

int parseInt(const WordIter& it, std::string msg) {
    return parseInt(it, msg, 0, it->size());
};

template<>
int parseCoeff<int>(const WordIter& it, size_t start, size_t length) {
    return parseInt(it, "Expected coefficient", start, length);
}

template<>
BigInt parseCoeff<BigInt>(const WordIter& word, size_t start, size_t length){
    assert(word->size() >= start + length);
    assert(length > 0);

    if (word.isEnd()) {
        throw ParseError(word, "Expected Number.");
    }

    const char* it = word->data() + start;
    if (*it == '+') {
        it += 1;
    }
    // copy to string to get \0 terminated string
    std::string subString(it, length);
    try {
        return BigInt(subString);
    } catch (...) {
        throw ParseError(word, "Error while parsing number.");
    }


}

class VariableNameManager {
    std::unordered_map<std::string, int> name2num;
    std::vector<std::string> num2name;
    size_t _maxVar = 0;

    bool allowArbitraryNames = false;

public:
    VariableNameManager(bool _allowArbitraryNames)
        : allowArbitraryNames(_allowArbitraryNames)
    {}

    size_t maxVar() {
        return std::max(_maxVar, num2name.size());
    }

    std::string getName(Var num) {
        if (!allowArbitraryNames) {
            return "x" + std::to_string(num);
        } else {
            return num2name[num - 1];
        }
    }

    Var getVar(const std::string_view& name) {
        if (name.size() < 2) {
            throw std::invalid_argument("Expected variable, but name is too short.");
        }

        if (!allowArbitraryNames) {
            if (name[0] != 'x') {
                throw std::invalid_argument("Expected variable, but did not start with 'x'.");
            }
            std::string var(name.substr(1));
            unsigned long varVal = std::stoul(var);
            if (varVal > Var::LIMIT) {
                throw std::invalid_argument("Variable too large, can not be represented.");
            }
            _maxVar = std::max(_maxVar, varVal);
            return Var(varVal);
        } else {
            if (!std::isalpha(name[0])) {
                throw std::invalid_argument("Expected variable, but did not start with letter.");
            }
            auto result = name2num.insert(std::make_pair(name, num2name.size()));
            bool newName = result.second;
            if (newName) {
                num2name.emplace_back(name);
            }
            size_t varVal = result.first->second + 1;
            if (varVal > Var::LIMIT) {
                throw std::invalid_argument("Too many variables, can not be represented.");
            }
            return Var(varVal);
        }
    }

    Lit getLit(const std::string_view& name) {
        if (name.size() < 1) {
            throw std::invalid_argument("Expected literal.");
        }

        bool isNegated = (name[0] == '~');
        int offset = isNegated?1:0;
        const std::string_view var = name.substr(offset);
        return Lit(this->getVar(var), isNegated);
    }
};

Lit parseLit(const WordIter& it, VariableNameManager& mngr, const std::string_view* value = nullptr) {
    if (it == WordIter::end) {
        throw ParseError(it, "Expected literal.");
    }

    try {
        if (value) {
            return mngr.getLit(*value);
        } else {
            return mngr.getLit(*it);
        }
    } catch (const std::invalid_argument& e) {
        throw ParseError(it, e.what());
    } catch (const std::out_of_range& e) {
        throw ParseError(it, e.what());
    }
}



class VarDouplicateDetection {
    std::vector<bool> contained;
    std::vector<Var> used;

public:
    bool add(Var var) {
        used.push_back(var);
        size_t idx = static_cast<size_t>(var);
        if (idx >= contained.size()) {
            contained.resize(idx + 1);
        }
        bool result = contained[idx];
        contained[idx] = true;
        return result;
    }

    void clear() {
        for (Var var: used) {
            contained[var] = false;
        }
        used.clear();
    }
};


template<typename T>
class Formula {
private:
    std::vector<std::unique_ptr<Inequality<T>>> constraints;

public:
    bool hasObjective = false;
    std::vector<size_t> objectiveVars;
    std::vector<T> objectiveCoeffs;

    size_t maxVar;
    size_t claimedNumC;
    size_t claimedNumVar;

    std::vector<Inequality<T>*> getConstraints() {
        std::vector<Inequality<T>*> result;
        result.reserve(constraints.size());
        for (auto& ptr: constraints) {
            result.push_back(ptr.get());
        }
        return result;
    }

    void add(std::unique_ptr<Inequality<T>>&& constraint) {
        constraints.emplace_back(std::move(constraint));
    }
};


template<typename T>
class OPBParser {
    VariableNameManager& variableNameManager;
    std::vector<Term<T>> terms;
    VarDouplicateDetection duplicateDetection;

    std::unique_ptr<Formula<T>> formula;

public:
    OPBParser(VariableNameManager& mngr):
        variableNameManager(mngr)
    {}


    std::unique_ptr<Formula<T>> parse(std::ifstream& f, const std::string& fileName) {
        formula = std::make_unique<Formula<T>>();
        WordIter it(fileName);
        if (!WordIter::getline(f, it)) {
            throw ParseError(it, "Expected OPB header.");
        }
        parseHeader(it);

        bool checkedObjective = false;
        while (WordIter::getline(f, it)) {
            if (it != WordIter::end && ((*it)[0] != '*')) {
                if (!checkedObjective) {
                    checkedObjective = true;
                    if (*it == "min:") {
                        ++it;
                        parseObjective(it);
                        continue;
                    }
                }

                auto res = parseConstraint(it);
                if (it != WordIter::end) {
                    throw ParseError(it, "Expected end line after constraint.");
                }
                formula->add(std::move(res[0]));
                if (res[1] != nullptr) {
                    formula->add(std::move(res[1]));
                }
            }
        }

        return std::move(formula);
    }

    void parseHeader(WordIter& it) {
        it.expect("*");
        ++it;
        it.expect("#variable=");
        ++it;
        formula->claimedNumVar = parseInt(it, "Expected number of variables.");
        ++it;
        it.expect("#constraint=");
        ++it;
        formula->claimedNumC = parseInt(it, "Expected number of constraints.");
        ++it;
        if (it != WordIter::end) {
            throw ParseError(it, "Expected end of header line.");
        }
    }

    void parseObjective(WordIter& it) {
        formula->hasObjective = true;
        while (it != WordIter::end && *it != ";") {
            T coeff = parseCoeff<T>(it, 0, it->size());
            ++it;
            Lit lit = parseLit(it, variableNameManager);

            if (lit.isNegated()) {
                coeff = -coeff;
                lit = ~lit;
            }

            formula->objectiveCoeffs.push_back(coeff);
            formula->objectiveVars.push_back(lit.var());

            ++it;
        }
    }

    std::array<std::unique_ptr<Inequality<T>>, 2> parseConstraint(WordIter& it, bool geqOnly = false) {
        terms.clear();

        T degreeOffset = 0;
        while (it != WordIter::end) {
            const string_view& word = *it;
            if (word == ">=" || word == "=") {
                break;
            }
            T coeff = parseCoeff<T>(it, 0, it->size());
            ++it;
            Lit lit = parseLit(it, variableNameManager);
            if (formula) {
                formula->maxVar = std::max(formula->maxVar, static_cast<size_t>(lit.var()));
            }
            if (duplicateDetection.add(lit.var())) {
                std::cout << lit.var() << std::endl;
                throw ParseError(it, "Douplicated variables are not supported in constraints.");
            }

            if (coeff < 0) {
                coeff = -coeff;
                degreeOffset += coeff;
                if (degreeOffset < 0) {
                    throw ParseError(it, "Overflow due to normalization.");
                }
                lit = ~lit;
            }

            terms.emplace_back(coeff, lit);
            ++it;
        }

        duplicateDetection.clear();

        it.expectOneOf({">=", "="});
        bool isEq = (*it == "=");
        if (isEq && geqOnly) {
            throw ParseError(it, "Equality not allowed, only >= is allowed here.");
        }

        ++it;
        if (it == WordIter::end) {
            throw ParseError(it, "Expected degree.");
        }
        assert(it->size() != 0);
        const string_view& rhs = *it;
        T degree;
        if (rhs[rhs.size() - 1] == ';') {
            degree = parseCoeff<T>(it, 0, rhs.size() - 1);
            ++it;
        } else {
            degree = parseCoeff<T>(it);
            ++it;
            it.expect(";");
            ++it;
        }

        T normalizedDegree = degree + degreeOffset;
        if (degree > normalizedDegree) {
            throw ParseError(it, "Overflow due to normalization.");
        }

        std::unique_ptr<Inequality<T>> geq = std::make_unique<Inequality<T>>(terms, normalizedDegree);
        std::unique_ptr<Inequality<T>> leq = nullptr;
        if (isEq) {
            normalizedDegree = -normalizedDegree;
            for (Term<T>& term:terms) {
                normalizedDegree += term.coeff;
                term.lit = ~term.lit;
            }

            leq = std::make_unique<Inequality<T>>(terms, normalizedDegree);
        }

        return {std::move(geq), std::move(leq)};
    }
};

template<typename T>
class CNFParser {
    VariableNameManager& variableNameManager;
    std::vector<Term<T>> terms;
    VarDouplicateDetection duplicateDetection;

    std::unique_ptr<Formula<T>> formula;

public:
    CNFParser(VariableNameManager& mngr):
        variableNameManager(mngr)
    {}


    std::unique_ptr<Formula<T>> parse(std::ifstream& f, const std::string& fileName) {
        formula = std::make_unique<Formula<T>>();
        WordIter it(fileName);

        bool foundHeader = false;
        while (WordIter::getline(f, it)) {
            if (it != WordIter::end && ((*it)[0] != 'c')) {
                if (!foundHeader) {
                    parseHeader(it);
                    foundHeader = true;
                } else {
                    auto res = parseConstraint(it);
                    formula->add(std::move(res));
                }
            }
        }

        if (!foundHeader) {
            throw ParseError(it, "Expected CNF header.");
        }

        return std::move(formula);
    }

    void parseHeader(WordIter& it) {
        it.expect("p");
        ++it;
        it.expect("cnf");
        ++it;
        formula->claimedNumVar = parseInt(it, "Expected number of variables.");
        ++it;
        formula->claimedNumC = parseInt(it, "Expected number of constraints.");
        ++it;
        if (it != WordIter::end) {
            throw ParseError(it, "Expected end of header line.");
        }
    }

    std::unique_ptr<Inequality<T>> parseConstraint(WordIter& it) {
        terms.clear();
        duplicateDetection.clear();

        char buffer[12];
        char* bufferEnd = buffer + 12;
        char* bufferStart;

        while (it != WordIter::end && *it != "0") {
            // parse int to give nice error messages if input is not
            // an integer, out put is not used because we construct
            // string to be consistent with arbitrary variable names
            parseInt(it, "Expected literal.");
            if (it->size() > 11) {
                throw ParseError(it, "Literal too large.");
            }
            bufferStart = bufferEnd - it->size();
            std::copy(it->begin(), it->end(), bufferStart);
            if (bufferStart[0] == '-') {
                bufferStart -= 1;
                bufferStart[0] = '~';
                bufferStart[1] = 'x';
            } else {
                bufferStart -= 1;
                bufferStart[0] = 'x';
            }

            std::string_view litString(bufferStart, bufferEnd - bufferStart);
            Lit lit = parseLit(it, variableNameManager, &litString);
            if (formula) {
                formula->maxVar = std::max(formula->maxVar, static_cast<size_t>(lit.var()));
            }

            terms.emplace_back(1, lit);

            // Note that douplicate variables will be handled during
            // construction of the Inequality that is if a literal
            // appears twice it will get coefficient 2, if a variable
            // appears with positive as well as negative polarity then
            // a cancelation will be triggered reducing the degree to
            // 0 and thereby trivializing the constraint.

            // todo: maybe add warning if douplicate variables occur?

            ++it;
        }
        if (*it != "0") {
            throw ParseError(it, "Expected '0' at end of clause.");
        }
        ++it;

        if (it != WordIter::end) {
            throw ParseError(it, "Expected end line after constraint.");
        }

        return std::make_unique<Inequality<T>>(terms, 1);
    }
};


template<typename T>
std::unique_ptr<Formula<T>> parseOpb(std::string fileName, VariableNameManager& varMgr) {
    std::ifstream f(fileName);
    OPBParser<T> parser(varMgr);
    std::unique_ptr<Formula<T>> result = parser.parse(f, fileName);
    return result;
}

template<typename T>
std::array<std::unique_ptr<Inequality<T>>, 2> parseOpbConstraint(VariableNameManager& varMgr, WordIter& it) {
    OPBParser<T> parser(varMgr);
    return parser.parseConstraint(it);
}


template<typename T>
std::unique_ptr<Formula<T>> parseCnf(std::string fileName, VariableNameManager& varMgr) {
    std::ifstream f(fileName);
    CNFParser<T> parser(varMgr);
    std::unique_ptr<Formula<T>> result = parser.parse(f, fileName);
    return result;
}

int main(int argc, char const *argv[])
{

    std::chrono::duration<double> timeAttach;
    std::chrono::duration<double> timeParse;
    std::chrono::duration<double> timeDetach;

    std::cout << "start reading file..." << std::endl;
    std::string proofFileName(argv[2]);
    std::string instanceFileName(argv[1]);
    std::ifstream proof(proofFileName);

    VariableNameManager manager(false);
    auto formula = parseCnf<CoefType>(instanceFileName, manager);
    PropEngine<CoefType> engine(formula->maxVar);
    std::vector<InequalityPtr<CoefType>> constraints;
    constraints.push_back(nullptr);
    uint64_t id = 0;
    for (auto* constraint: formula->getConstraints()) {
        engine.attach(constraint, ++id);
        constraints.push_back(nullptr);
    }

    uint64_t formulaIds = id;

    size_t rupSteps = 0;
    size_t nLits = 0;
    size_t delSteps = 0;
    size_t count = 0;
    WordIter it(argv[1]);
    while (WordIter::getline(proof, it)) {
        if (it.get() == "pseudo-Boolean") {

        } else if (it.get() == "f") {
        } else if (it.get() == "u") {
            rupSteps += 1;
            it.next();
            auto c = parseOpbConstraint<CoefType>(manager, it);
            if (!c[0]->rupCheck(engine)) {
                std::cout << "Verification Failed." << std::endl;
                return 1;
            };
            // nLits += c[0]->size();
            Inequality<CoefType>* ineq;
            {
                Timer timer(timeAttach);
                ineq = engine.attach(c[0].get(), ++id);
            }

            if (ineq == c[0].get()) {
                constraints.emplace_back(std::move(c[0]));
            } else {
                constraints.push_back(nullptr);
            }

        } else if (it.get() == "del") {
            delSteps += 1;
            it.next();
            it.next();
            auto c = parseOpbConstraint<CoefType>(manager, it);
            auto ineq = engine.find(c[0].get());
            if (ineq == nullptr) {
                std::cout << "Verification Failed." << std::endl;
                return 1;
            }
            assert(ineq->ids.size() > 0);
            uint64_t id = 0;
            for (uint64_t tid: ineq->ids) {
                id = tid;
                if (id != ineq->minId) {
                    break;
                }
            }
            {
                Timer timer(timeDetach);
                engine.detach(ineq, id);
            }
            if (ineq->ids.size() == 0) {
                assert(id <= formulaIds || constraints[id] != nullptr);
                constraints[id] = nullptr;
            }

        } else if (it.get() == "c") {
        } else {
            while (!it.isEnd()) {
                std::cout << it.get() << " ";
                it.next();
            }
            count += 1;
            std::cout << "\n";
        }
    }

    std::cout << "unkonwn lines:" << count << std::endl;

    std::cout << "avgSize:" << ((float) nLits) / rupSteps << std::endl;

    std::cout << "rupSteps: " << rupSteps << std::endl;
    std::cout << "delSteps: " << delSteps << std::endl;

    std::cout << "c statistic: time attach: "
        << std::fixed << std::setprecision(2)
        << timeAttach.count() << std::endl ;

    std::cout << "c statistic: time detach: "
        << std::fixed << std::setprecision(2)
        << timeDetach.count() << std::endl ;

    engine.printStats();
    return 0;
}

#ifdef PY_BINDINGS
void init_parsing(py::module &m){
    m.doc() = "Efficient implementation for parsing opb and pbp files.";
    m.def("parseOpb", &parseOpb<CoefType>, "Parse opb file with fixed precision.");
    m.def("parseOpbBigInt", &parseOpb<BigInt>, "Parse opb file with arbitrary precision.");
    m.def("parseCnf", &parseCnf<CoefType>, "Parse cnf file with fixed precision.");
    m.def("parseCnfBigInt", &parseCnf<BigInt>, "Parse cnf file with arbitrary precision.");

    m.def("parseConstraintOpb", &parseOpbConstraint<CoefType>, "Parse opb consraint with fixed precision.");
    m.def("parseConstraintOpbBigInt", &parseOpbConstraint<BigInt>, "Parse opb constraint with arbitrary precision.");

    py::register_exception_translator([](std::exception_ptr p) {
        try {
            if (p) std::rethrow_exception(p);
        } catch (const ParseError &e) {
            py::object pyParseError = py::module::import("veripb.exceptions").attr("DirectParseError");
            PyErr_SetString(pyParseError.ptr(), e.what());
        }
    });

    py::class_<VariableNameManager>(m, "VariableNameManager")
        .def(py::init<bool>())
        .def("maxVar", &VariableNameManager::maxVar)
        .def("getVar", [](VariableNameManager &mngr, std::string name) {
            return static_cast<uint64_t>(mngr.getVar(name));
        })
        .def("getLit", [](VariableNameManager &mngr, std::string name) {
            return static_cast<int64_t>(mngr.getLit(name));
        })
        .def("getName", [](VariableNameManager &mngr, LitData value) {
            return mngr.getName(Var(value));
        });

    py::class_<std::ifstream>(m, "ifstream")
        .def(py::init<std::string>())
        .def("close", &std::ifstream::close);

    m.def("nextLine", &nextLine);

    py::class_<WordIter>(m, "WordIter")
        .def(py::init<std::string>())
        .def("next", &WordIter::operator++)
        .def("get", &WordIter::get)
        .def("isEnd", &WordIter::isEnd)
        .def("getFileName", &WordIter::getFileName)
        .def("getLine", &WordIter::getLine)
        .def("getLineText", &WordIter::getLineText)
        .def("getColumn", &WordIter::getColumn);

    py::class_<Formula<CoefType>>(m, "Formula")
        .def(py::init<>())
        .def("getConstraints", &Formula<CoefType>::getConstraints,
            py::return_value_policy::reference_internal)
        .def_readonly("maxVar", &Formula<CoefType>::maxVar)
        .def_readonly("claimedNumC", &Formula<CoefType>::claimedNumC)
        .def_readonly("claimedNumVar", &Formula<CoefType>::claimedNumVar)
        .def_readonly("hasObjective", &Formula<CoefType>::hasObjective)
        .def_readonly("objectiveVars", &Formula<CoefType>::objectiveVars)
        .def_readonly("objectiveCoeffs", &Formula<CoefType>::objectiveCoeffs);

    py::class_<Formula<BigInt>>(m, "FormulaBigInt")
        .def(py::init<>())
        .def("getConstraints", &Formula<BigInt>::getConstraints,
            py::return_value_policy::reference_internal)
        .def_readonly("maxVar", &Formula<BigInt>::maxVar)
        .def_readonly("claimedNumC", &Formula<BigInt>::claimedNumC)
        .def_readonly("claimedNumVar", &Formula<BigInt>::claimedNumVar)
        .def_readonly("hasObjective", &Formula<BigInt>::hasObjective)
        .def_readonly("objectiveVars", &Formula<BigInt>::objectiveVars)
        .def_readonly("objectiveCoeffs", &Formula<BigInt>::objectiveCoeffs);
}
#endif