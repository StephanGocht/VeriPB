#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <cctype>

typedef std::string_view string_view;

// todo: fix this!!!
#include "constraints.cpp"

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
    bool isEnd = false;

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
    WordIter():fileInfo("WordIter::EndItterator"){isEnd = true;}

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

    bool hasNext() const {
        return start != string_view::npos;
    }

    friend bool operator==(const WordIter& a, const WordIter& b) {
        if (a.isEnd) {
            return (b.start == string_view::npos);
        } else if (b.isEnd) {
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
    std::cout  << it->size() << std::endl;
    return parseCoeff<T>(it, 0, it->size());
};

template<typename T>
T parseCoeff(const WordIter& it, size_t start, size_t length);

int parseInt(const WordIter& word, std::string msg, size_t start, size_t length) {
    assert(word->size() >= start + length);
    assert(length > 0);

    if (!word.hasNext()) {
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

public:
    std::string getName(Var num) {
        return num2name[num - 1];
    }

    Var getVar(const WordIter& it, size_t start, size_t size) {
        return Var(parseInt(it, "", 1, size - 1));
    }

    Var getVar(const std::string& name) {
        return Var(std::stoi(name.substr(1, name.size() - 1)));
    }

    // Var getVar(const std::string& name) {
    //     auto result = name2num.insert(std::make_pair(name, num2name.size()));
    //     bool newName = result.second;
    //     if (newName) {
    //         num2name.push_back(name);
    //     }
    //     return Var(result.first->second + 1);
    // }

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



template<typename T>
class OPBParser {
    VariableNameManager& variableNameManager;
    std::vector<Term<T>> terms;
    int numVar = 0;
    int numC = 0;

public:
    OPBParser(VariableNameManager& mngr):
        variableNameManager(mngr)
    {}


    std::vector<Inequality<T>*> parse(std::ifstream& f, const std::string& fileName) {
        WordIter it(fileName);
        if (!WordIter::getline(f, it)) {
            throw ParseError(it, "Expected OPB header.");
        }
        parseHeader(it);

        std::vector<Inequality<T>*> result;

        while (WordIter::getline(f, it)) {
            if (it != WordIter::end && ((*it)[0] != '*')) {
                std::array<Inequality<T>*, 2> res = parseConstraint(it);
                result.push_back(res[0]);
                if (res[1] != nullptr) {
                    result.push_back(res[1]);
                }
            }
        }

        return result;
    }

    void parseHeader(WordIter& it) {
        it.expect("*");
        ++it;
        it.expect("#variable=");
        ++it;
        numVar = parseInt(it, "Expected number of variables.");
        ++it;
        it.expect("#constraint=");
        ++it;
        numC = parseInt(it, "Expected number of constraints.");
        ++it;
        if (it != WordIter::end) {
            throw ParseError(it, "Expected end of header line.");
        }
    }

    std::array<Inequality<T>*, 2> parseConstraint(WordIter& it, bool geqOnly = false) {
        terms.clear();
        while (it != WordIter::end) {
            const string_view& word = *it;
            if (word == ">=" || word == "=") {
                break;
            }
            T coeff = parseCoeff<T>(it, 0, it->size());
            ++it;
            Lit lit = parseLit(it, variableNameManager);
            ++it;

            terms.emplace_back(coeff, lit);
        }

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

        return {nullptr, nullptr};
    }
};


int main(int argc, char const *argv[])
{
    std::string fileName(argv[1]);
    std::ifstream f(fileName);


    VariableNameManager manager;
    OPBParser<int> parser(manager);
    try {
        std::cout << parser.parse(f, fileName).size() << std::endl;
    } catch (const ParseError& e) {
        std::cout << e.what() << std::endl;
    }
    // try {
    //     int i = 0;
    //     WordIter it(argv[1]);
    //     while (WordIter::getline(f, it)) {
    //         while (it != WordIter::endIt) {
    //             // std::cout << *it << std::endl;
    //             i+= it->size();
    //             // word = *it;
    //             // if (i > 10) {
    //             //     throw ParseError(it, "test parse error");
    //             // }
    //             ++it;
    //         }
    //     }
    //     std::cout << i << std::endl;
    // } catch (const ParseError& e) {
    //     std::cout << e.what() << std::endl;
    // }
    return 0;
}