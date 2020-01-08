#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

struct FileInfo {
    FileInfo(std::string _fileName)
        : fileName(_fileName)
    {}

    std::string fileName;
    size_t line = 0;
};

class WordIter {
public:
    FileInfo fileInfo;
    std::string line;
    std::string word;
    size_t start = 0;
    size_t end = 0;
    size_t nextStart = 0;
    bool isEnd = false;

    void next() {
        start = nextStart;
        if (start != std::string::npos) {
            end = line.find_first_of(" ", start);
            if (end == std::string::npos) {
                nextStart = end;
            } else {
                nextStart = line.find_first_not_of(" ", end);
            }

            word = line.substr(start, end - start);
        } else {
            word = "";
        }
    }

private:
    WordIter():fileInfo("WordIter::EndItterator"){isEnd = true;}

public:
    static WordIter endIt;

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
        end = 0;
        nextStart = line.find_first_not_of(" ", end);
        next();
    }

    const std::string& operator*() const {return word;};
    const std::string* operator->() const {return &word;};

    WordIter& operator++() {
        next();
        return *this;
    }

    friend bool operator==(const WordIter& a, const WordIter& b) {
        if (a.isEnd) {
            return (b.start == std::string::npos);
        } else if (b.isEnd) {
            return (a.start == std::string::npos);
        } else {
            return (a.line == b.line) and (a.start == b.start) and (a.end == b.end);
        }

    }
    friend bool operator!=(const WordIter& a, const WordIter& b) {
        return !(a==b);
    }
};

WordIter WordIter::endIt;

class ParseError: std::runtime_error {
public:
    std::string fileName = "";
    size_t line = 0;
    size_t column = 0;

    explicit ParseError(const WordIter& it, const std::string& what_arg)
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




int main(int argc, char const *argv[])
{
    std::ifstream f(argv[1]);

    // std::string word;
    // int i = 0;
    // while (f >> word) {
    //     i+= word.size();
    // }
    // std::cout << i << std::endl;

    try {
        int i = 0;
        WordIter it(argv[1]);
        while (WordIter::getline(f, it)) {
            while (it != WordIter::endIt) {
                // std::cout << *it << std::endl;
                i+= it->size();
                // word = *it;
                // if (i > 10) {
                //     throw ParseError(it, "test parse error");
                // }
                ++it;
            }
        }
        std::cout << i << std::endl;
    } catch (const ParseError& e) {
        std::cout << e.what() << std::endl;
    }
    return 0;
}