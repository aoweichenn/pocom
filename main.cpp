//
// Created by aowei on 2025/9/15.
//

#include <lexer/regex/engine.hpp>

int main() {
    const std::string p = lexer::regex::preprocess_regex("a(a|b)*");
    const auto tokens = lexer::regex::lexer(p);
    const auto postfix = lexer::regex::infix_to_postfix(tokens);
    const auto nfa = lexer::regex::build_nfa(postfix);
    nfa->print("Original NFA");
    return 0;
}
