#pragma once

#include <sstream>
#include <iostream>
#include <fstream>
#include <functional>

#define EOM \
    logging::EndOfMessage(__FILE__, __LINE__)

#define LOG(X) \
    if (logging::use::X) logging::Streams::X()


namespace logging {
    class LogStream: public std::ostringstream {
    private:
        std::string start;
        std::ostream& stream;
    public:
        LogStream(std::string start, std::ostream& _stream = std::cout)
            : stream(_stream)
        {
            this->start = start;
        }

        virtual void endOfMessage(const char* file, uint line) {
            std::string fileName(file);
            size_t pos = fileName.find_last_of("/") + 1;

            std::ostringstream startOfLine;
            startOfLine << start << ":" << fileName.substr(pos) << ":" << line << ": ";

            std::string message = this->str();

            size_t start = 0;
            size_t end = std::min(message.find("\n", start), message.size());
            if (message.size() == 0) {
                stream << startOfLine.str() << std::endl;
            }
            while (start < message.size()) {
                stream << startOfLine.str() << message.substr(start, end - start) << std::endl;
                start = end + 1;
                end = std::min(message.find("\n", start), message.size());
            }

            this->str("");
            this->clear();
        };
    };

    // class Wrapper {
    // private:
    //     std::function<void(std::ostream& log)> f;
    // public:
    //     Wrapper(std::function<void(std::ostream& log)> _f)
    //         :f(_f)
    //     {}

    //     friend std::ostream& operator<<(std::ostream& stream, const Wrapper& wrapper);
    // };

    // std::ostream& operator<<(std::ostream& stream, const Wrapper& wrapper) {
    //     wrapper.f(stream);
    //     return stream;
    // }

    class FatalLogException: public std::exception {};

    class FatalLogStream: public LogStream {
    public:
        FatalLogStream(std::string start):
            LogStream(start) {
        }

        virtual void endOfMessage(const char* file, uint line) {
            LogStream::endOfMessage(file, line);
            throw FatalLogException();
        };
    };

    class EndOfMessage {
    private:
        const char* file;
        uint line;
    public:
        EndOfMessage(const char* file, uint line) {
            this->file = file;
            this->line = line;
        }

        friend void operator<< (std::ostream& stream, const EndOfMessage& eom) {
            static_cast<LogStream&>(stream).endOfMessage(eom.file, eom.line);
        }
    };

    class Streams {
    public:
        static LogStream& err() {
            static LogStream log("ERROR");
            return log;
        };
        static LogStream& debug() {
            static LogStream log("DEBUG");
            return log;
        };
        static FatalLogStream& fatal() {
            static FatalLogStream log("FATAL");
            return log;
        };
        static LogStream& info() {
            static LogStream log("INFO");
            return log;
        };
        static LogStream& warn() {
            static LogStream log("WARNING");
            return log;
        };
        static LogStream& trace() {
            static LogStream log("TRACE");
            return log;
        };
        static LogStream& stat() {
            static std::ofstream statFile("stats.txt");
            static LogStream log("STATS", statFile);
            return log;
        };
    };

    namespace use {
        static const bool err = true;
        static const bool fatal = true;
        static const bool info = false;
        static const bool warn = true;
        static const bool stat = false;
        static const bool trace = false;
        #if defined(NDEBUG)
            static const bool debug = false;
        #else
            static const bool debug = true;
        #endif
    }
}
