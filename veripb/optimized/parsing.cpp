#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <cctype>
#include "constraints.h"

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
    FileInfo fileInfo;
    std::string line;
    string_view word;
    size_t start = 0;
    size_t endPos = 0;
    size_t nextStart = 0;
    bool isEndIt = false;

    void next() {
        start = nextStart;
        if (start != string_view::npos) {
            endPos = line.find_first_of(" ", start);
            if (endPos == string_view::npos) {
                nextStart = endPos;
                endPos = line.size();
            } else {
                nextStart = line.find_first_not_of(" ", endPos);
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
        nextStart = line.find_first_not_of(" ", endPos);
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
        return start != string_view::npos;
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
};

WordIter WordIter::end;

ParseError::ParseError(const WordIter& it, const std::string& what_arg)
    : std::runtime_error(what_arg)
{
    fileName = it.fileInfo.fileName;
    line = it.fileInfo.line;
    if (it.start == std::string::npos) {
        column = it.line.size() + 1;
    } else {
        column = it.start + 1;
    }
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

    if (!word.isEnd()) {
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
        char chr = *it - '0';
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

class VariableNameManager {
    std::unordered_map<std::string, int> name2num;
    std::vector<std::string> num2name;
    size_t _maxVar = 0;

    bool allowArbitraryNames = false;

public:
    VariableNameManager(bool _allowArbitraryNames)
        : allowArbitraryNames(_allowArbitraryNames)
    {}

    size_t pyGetVar(std::string name) {
        return getVar(name);
    }

    std::string pyGetName(size_t num) {
        return getName(Var(num));
    }

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

    Var getVar(const WordIter& it, size_t start, size_t size) {
        if (!allowArbitraryNames) {
            size_t var = parseInt(it, "", start + 1, size - 1);
            _maxVar = std::max(_maxVar, var);
            return Var(var);
        } else {
            return getVar(std::string(it->data() + start, size));
        }
    }

    Var getVar(const std::string& name) {
        if (!allowArbitraryNames) {
            size_t var = std::stoi(name.substr(1, name.size() - 1));
            _maxVar = std::max(_maxVar, var);
            return Var(var);
        } else {
            auto result = name2num.insert(std::make_pair(name, num2name.size()));
            bool newName = result.second;
            if (newName) {
                num2name.push_back(name);
            }
            return Var(result.first->second + 1);
        }
    }

    bool isLit(const string_view& name) {
        if (name.size() < 1) {
            return false;
        }

        if (name[0] == '~') {
            return isVar(name, 1);
        } else {
            return isVar(name);
        }
    }

    bool isVar(const string_view& name, size_t pos = 0) {
        if (name.size() >= 2
                && std::isalpha(name[pos])) {
            return true;
        } else {
            return false;
        }
    }
};

Lit parseLit(const WordIter& it, VariableNameManager& mngr) {
    if (it == WordIter::end || !mngr.isLit(*it)) {
        throw ParseError(it, "Expected literal.");
    }

    bool isNegated = ((*it)[0] == '~');
    size_t start = 0;
    if (isNegated) {
        start += 1;
    }

    return Lit(mngr.getVar(it, start, it->size() - start), isNegated);
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
std::unique_ptr<Formula<T>> parseOpb(std::string fileName, VariableNameManager& varMgr) {
    std::ifstream f(fileName);
    OPBParser<CoefType> parser(varMgr);
    std::unique_ptr<Formula<T>> result = parser.parse(f, fileName);
    return result;
}

int main(int argc, char const *argv[])
{

    std::cout << "start reading file..." << std::endl;
    std::string fileName(argv[1]);
    std::ifstream f(fileName);


    VariableNameManager manager(false);
    OPBParser<int> parser(manager);
    try {
        std::cout << parser.parse(f, fileName)->getConstraints().size() << std::endl;
    } catch (const ParseError& e) {
        std::cout << e.what() << std::endl;
    }
    return 0;
}

#ifdef PY_BINDINGS
void init_parsing(py::module &m){
    m.doc() = "Efficient implementation for parsing opb and pbp files.";
    m.def("parseOpb", &parseOpb<CoefType>, "Parse opb file");

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
        .def("getVar", &VariableNameManager::pyGetVar)
        .def("getName", &VariableNameManager::pyGetName);

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
}
#endif