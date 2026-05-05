#pragma once
#include <antlr4-runtime.h>
#include <iostream>
#include <string>
#include <vector>

namespace luv {

class LuvErrorListener : public antlr4::BaseErrorListener {
public:
    void syntaxError(antlr4::Recognizer *recognizer, antlr4::Token *offendingSymbol,
                     size_t line, size_t charPositionInLine, const std::string &msg,
                     std::exception_ptr e) override {
        
        std::cerr << "\033[1;31mError at line " << line << ":" << charPositionInLine << "\033[0m" << std::endl;
        std::cerr << "  \033[1;37m" << msg << "\033[0m" << std::endl;

        if (msg.find("expecting") != std::string::npos) {
            std::cerr << "  \033[1;34mAdvice: Check if you missed a delimiter or typed a keyword incorrectly.\033[0m" << std::endl;
        } else if (msg.find("mismatched input") != std::string::npos) {
            std::cerr << "  \033[1;34mAdvice: This token is unexpected here. Review your syntax structure.\033[0m" << std::endl;
        }
        
        exit(1);
    }
};

} // namespace luv
