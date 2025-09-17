//
// Created by aowei on 2025/9/15.
//

#include <algorithm>
#include <iostream>
#include <lexer/regex/engine.hpp>

int main() {
    const std::string p = lexer::regex::preprocess_regex("a(c|b)*");
    const auto tokens = lexer::regex::lexer(p);
    const auto postfix = lexer::regex::infix_to_postfix(tokens);
    const auto nfa = lexer::regex::build_nfa(postfix);
    const auto dfa = lexer::regex::build_dfa(std::move(nfa));
    const auto min_dfa = lexer::regex::minimize_dfa(*dfa);
    min_dfa->print();
    const bool is_match = lexer::regex::match(*min_dfa, "abcbcbcbcbbcbbbccc");
    std::cout << std::boolalpha << is_match << std::endl;
    return 0;
}
