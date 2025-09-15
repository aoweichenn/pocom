//
// Created by aowei on 2025/9/15.
//

#ifndef POCOM_LEXERL_CASES_IDENTIFIER_HPP
#define POCOM_LEXERL_CASES_IDENTIFIER_HPP

#include <map>
#include <set>
#include <vector>
#include <memory>

namespace lexer::cases {
    struct State {
        int id;                                         // 状态唯一 ID，用于调试和 hash
        std::map<char, std::set<State *> > transitions; // 普通转移: 字符 --> 目标状态集合（裸指针 = 引用）
        std::set<State *> eps_transitions;              // eps(空)转移：无字符 --> 目标状态集合（裸指针 = 引用）

        explicit State(const int id) : id(id) {}
    };

    struct NFA {
        std::vector<std::unique_ptr<State> > states; // 核心：State 的唯一所有者(RAII)
        State *start;                                // 初始状态（裸指针 = 引用 states 中的对象）
        State *accept;                               // 接收状态（裸指针 = 引用 states 中的对象）

        NFA() : start(nullptr), accept(nullptr) {}
    };

    struct DFAState {
        int id;                       // DFA 状态唯一 ID
        std::set<State *> nfa_states; // 对应 NFA 的状态集合（裸指针 = 引用，不拥有数据的管理权限）
        static int id_counter;        // 静态计数器：自动分配 ID

        explicit DFAState(const std::set<State *> &nfa_states) : id(id_counter++), nfa_states(nfa_states) {}

        [[nodiscard]] size_t hash() const;
    };
}

namespace lexer::cases {}

namespace lexer::cases {
    NFA build_identifier_nfa();
}
#endif //POCOM_LEXERL_CASES_IDENTIFIER_HPP
