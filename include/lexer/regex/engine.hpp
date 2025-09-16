//
// Created by aowei on 2025 9月 16.
//

#ifndef POCOM_ENGINE_HPP
#define POCOM_ENGINE_HPP

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>

// 全局常量部分
namespace lexer::regex {
    // 全局常量：表示 ε 转移，代替 '\0'，提升可读性
    constexpr char EPSILON = '\0';
}

// 1. 词法单元定义
namespace lexer::regex {
    enum class TokenType {
        CHAR,   // 普通字符
        STAR,   // * 闭包
        OR,     // | 选择
        LPAREN, // ( 左括号
        RPAREN, // ) 又括号
        CONCAT, // 隐含连接符，仅供内部使用
    };

    struct Token {
        TokenType type;
        char value; // 仅 CHAR 类型有效（经过转义后的值）
    };
}

// 2. NFA 相关定义
namespace lexer::regex {
    struct NFAState {
        bool is_accept;
        const int id;
        std::unordered_map<char, std::vector<NFAState *> > transitions;

        explicit NFAState(bool is_accept);
    };

    class NFA {
    public:
        NFAState *start = nullptr;
        NFAState *end = nullptr;
        std::vector<std::unique_ptr<NFAState> > states;

    public:
        NFA() = default;
        NFA(const NFA &) = delete;
        NFA &operator=(const NFA &) = delete;
        NFA(NFA &&) = default;
        NFA &operator=(NFA &&) = default;

        // 添加状态并返回原始指针，所有权保留在 NFA 中
        NFAState *add_state(std::unique_ptr<NFAState> state);
        // 调试用：打印 NFA 结构
        void print(const std::string &name = "NFA") const;
    };
}

// NFAState* 的 hash 函数，用于加入到 unordered_set 里面
template<>
struct std::hash<std::unordered_set<lexer::regex::NFAState *> > {
    size_t operator()(const std::unordered_set<lexer::regex::NFAState *> &state_set) const noexcept {
        size_t hash_value = 0;
        for (const auto *state: state_set) {
            hash_value ^= std::hash<int>()(state->id);
        }
        return hash_value;
    }
};

// 3. DFA 相关定义
namespace lexer::regex {
    struct DFAState {
        bool is_accept;
        const int id;
        std::unordered_map<char, DFAState *> transitions;

        explicit DFAState(bool is_accept);
    };

    class DFA {
    public:
        DFAState *start = nullptr;
        std::vector<std::unique_ptr<DFAState> > states;
        std::unordered_map<std::unordered_set<NFAState *>, DFAState *> state_map;

    public:
        DFA() = delete;
        DFA(const DFA &) = delete;
        DFA &operator=(const DFA &) = delete;
        DFA(DFA &&) = default;
        DFA &operator=(DFA &&) = default;

        // 添加状态到状态列表里面，并返回原始指针
        DFAState *add_state(std::unique_ptr<DFAState> state);
        // 调试用：打印 DFA 结构
        void print(const std::string &name = "DFA") const;
    };
}

// 4.核心功能函数的声明
namespace lexer::regex {
    // 预处理：处理正则表达式中的转义字符，例如 \. -> .
    std::string preprocess_regex(const std::string_view &raw_regex);
    // 词法分析：预处理后的正则 -> Token 列表
    std::vector<Token> lexer(std::string_view processed_regex);
    // 语法分析：Token 列表 -> 后缀表达式
    std::string infix_to_postfix(const std::vector<Token> &tokens);
    // NFA 构建：后缀表达式 -> NFA
    std::unique_ptr<NFA> build_nfa(const std::string &postfix);
    // DFA 构建: NFA -> DFA ，NFA 所有权转移到 DFA
    std::unique_ptr<DFA> build_dfa(std::unique_ptr<NFA> nfa);
    // DFA 最小化：原始 DFA -> 最小 DFA
    std::unique_ptr<DFA> minimize_dfa(const DFA &original_dfa);
    // 匹配：最小 DFA + 输入字符串 -> 是否完全匹配
    bool match(const DFA &dfa, std::string_view input);
    // 工具函数：重置状态 ID 计数器，用于多次测试/构建
    void reset_state_counters();
}

#endif //POCOM_ENGINE_HPP
