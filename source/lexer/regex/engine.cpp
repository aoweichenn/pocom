//
// Created by aowei on 2025 9月 16.
//

#include <iostream>
#include <atomic>
#include <stack>

#include <lexer/regex/engine.hpp>

// 匿名命名空间：状态 ID 计数器（线程安全）
namespace lexer::regex {
    namespace {
        std::atomic<int> nfa_state_id_counter{0};
        std::atomic<int> dfa_state_id_counter{0};
    }
}

// NFA 的实现
namespace lexer::regex {
    // NFASate 构造函数
    NFAState::NFAState(const bool is_accept) : is_accept(is_accept), id(nfa_state_id_counter++) {}

    // NFA 状态添加函数
    NFAState *NFA::add_state(std::unique_ptr<NFAState> state) {
        this->states.emplace_back(std::move(state));
        return this->states.back().get();
    }

    // 调试用打印函数
    void NFA::print(const std::string &name) const {
        std::cout << "=== " << name << " Structure ===" << std::endl;
        std::cout << "Start State: " << (start ? std::to_string(start->id) : "None") << std::endl;
        std::cout << "Accept States: ";
        for (const auto &s: states) {
            if (s->is_accept) std::cout << s->id << " ";
        }
        std::cout << "\nTransitions:\n";
        for (const auto &s: states) {
            for (const auto &[c, targets]: s->transitions) {
                std::string char_str = (c == EPSILON) ? "ε" : std::string(1, c);
                for (const auto *t: targets) {
                    std::cout << "  State " << s->id << " --" << char_str << "--> State " << t->id << std::endl;
                }
            }
        }
        std::cout << "===========================\n" << std::endl;
    }
}

// DFA 的实现
namespace lexer::regex {
    // DFAState 构造函数
    DFAState::DFAState(const bool is_accept) : is_accept(is_accept), id(dfa_state_id_counter++) {}

    // DFA 状态添加函数
    DFAState *DFA::add_state(std::unique_ptr<DFAState> state) {
        this->states.emplace_back(std::move(state));
        return this->states.back().get();
    }

    // DFA 调试用打印函数
    void DFA::print(const std::string &name) const {
        std::cout << "=== " << name << " Structure ===" << std::endl;
        std::cout << "Start State: " << (start ? std::to_string(start->id) : "None") << std::endl;
        std::cout << "Accept States: ";
        for (const auto &s: states) {
            if (s->is_accept) std::cout << s->id << " ";
        }
        std::cout << "\nTransitions:\n";
        for (const auto &s: states) {
            for (const auto &[c, target]: s->transitions) {
                std::cout << "  State " << s->id << " --" << c << "--> State " << target->id << std::endl;
            }
        }
        std::cout << "===========================\n" << std::endl;
    }
}

// NFA 构建辅助函数
namespace lexer::regex {
    namespace {
        // 单个字符的构建：c
        std::unique_ptr<NFA> create_char_nfa(const char c) {
            auto nfa = std::make_unique<NFA>();
            auto start = std::make_unique<NFAState>(false);
            auto end = std::make_unique<NFAState>(false);
            NFAState *startptr = nfa->add_state(std::move(start));
            NFAState *endptr = nfa->add_state(std::move(end));
            startptr->transitions[c].push_back(endptr);
            nfa->start = startptr;
            nfa->end = endptr;
            return nfa;
        }

        // 连接的构建：ab
        std::unique_ptr<NFA> create_concatenate_nfa(std::unique_ptr<NFA> &&a, std::unique_ptr<NFA> &&b) {
            auto nfa = std::make_unique<NFA>();
            // 转移动 a 和 b 的状态所有权到新的 NFA
            std::move(a->states.begin(), a->states.end(), std::back_inserter(nfa->states));
            std::move(b->states.begin(), b->states.end(), std::back_inserter(nfa->states));
            // a 的接受状态 ->ε-> b 的起始状态，且 a 的接受状态失效
            a->end->is_accept = false;
            a->end->transitions[EPSILON].push_back(b->start);
            // 新的 NFA 的起始/接受状态
            nfa->start = a->start;
            nfa->end = b->end;
            return nfa;
        }

        // 选择的构建：a|b
        std::unique_ptr<NFA> create_alternative_nfa(std::unique_ptr<NFA> &&a, std::unique_ptr<NFA> &&b) {
            auto nfa = std::make_unique<NFA>();
            // 新的起始/接收状态
            auto new_start = std::make_unique<NFAState>(false);
            auto new_end = std::make_unique<NFAState>(true);
            NFAState *startptr = nfa->add_state(std::move(new_start));
            NFAState *endptr = nfa->add_state(std::move(new_end));
            // 转移 a 和 b 的状态
            std::move(a->states.begin(), a->states.end(), std::back_inserter(nfa->states));
            std::move(b->states.begin(), b->states.end(), std::back_inserter(nfa->states));
            // ε 转移：新的起始 -> a/b 的起始；a/b 的接受 -> 新的接受
            startptr->transitions[EPSILON].push_back(a->start);
            startptr->transitions[EPSILON].push_back(b->start);
            a->end->is_accept = false;
            b->end->is_accept = false;
            a->end->transitions[EPSILON].push_back(endptr);
            b->end->transitions[EPSILON].push_back(endptr);
            // 绑定状态
            nfa->start = startptr;
            nfa->end = endptr;
            return nfa;
        }

        // 闭包的构建：a*
        std::unique_ptr<NFA> create_kleene_closure(std::unique_ptr<NFA> &&a) {
            auto nfa = std::make_unique<NFA>();
            // 新的起始/接受状态
            auto new_start = std::make_unique<NFAState>(false);
            auto new_end = std::make_unique<NFAState>(true);
            NFAState *startptr = nfa->add_state(std::move(new_start));
            NFAState *endptr = nfa->add_state(std::move(new_end));
            // 转移 a 的状态
            std::move(a->states.begin(), a->states.end(), std::back_inserter(nfa->states));
            // ε 转移：新起始 -> a 的起始/新的接受；a 的接受 -> a 的起始/新的接受
            startptr->transitions[EPSILON].push_back(a->start);
            startptr->transitions[EPSILON].push_back(endptr);
            a->end->is_accept = false;
            a->end->transitions[EPSILON].push_back(a->start);
            a->end->transitions[EPSILON].push_back(endptr);
            // 绑定状态
            nfa->start = startptr;
            nfa->end = endptr;
            return nfa;
        }
    }
}

// 核心功能函数实现
namespace lexer::regex {
    // 预处理转义字符
    std::string preprocess_regex(const std::string_view &raw_regex) {
        std::string processed;
        for (size_t i = 0; i < raw_regex.size(); ++i) {
            if (raw_regex[i] == '\\' && i + 1 < raw_regex.size()) {
                processed += raw_regex[++i];
            } else {
                processed += raw_regex[i];
            }
        }
        if (processed.empty()) {
            throw std::invalid_argument("Empty regex is not supported.");
        }
        return processed;
    }

    // 词法分析：预处理后的正则 -> Token 列表
    std::vector<Token> lexer(std::string_view processed_regex) {
        std::vector<Token> tokens;
        const size_t length = processed_regex.size();
        for (std::size_t i = 0; i < length; ++i) {
            const char c = processed_regex[i];
            switch (c) {
                case '*':
                    tokens.push_back({TokenType::STAR, '\0'});
                    break;
                case '|':
                    tokens.push_back({TokenType::OR, '\0'});
                    break;
                case '(':
                    tokens.push_back({TokenType::LPAREN, '\0'});
                    break;
                case ')':
                    tokens.push_back({TokenType::RPAREN, '\0'});
                    break;
                default:
                    // 隐含字符（允许字母、数字、标点和空格等）
                    tokens.push_back({TokenType::CHAR, c});
                    // 自动插入隐含连接符号（例如 "ab" -> "a.CONCAT.b"）
                    if (i + 1 < length) {
                        const char next_c = processed_regex[i + 1];
                        if (std::isalnum(next_c) || next_c == '(' || next_c == '\\') {
                            tokens.push_back({TokenType::CONCAT, '\0'});
                        }
                    }
                    break;
            }
        }
        return tokens;
    }

    // 语法分析：Token 列表 -> 后缀表达式，调度场算法
    std::string infix_to_postfix(const std::vector<Token> &tokens) {
        std::string postfix;
        std::stack<TokenType> op_stack;
        // 运算法优先级：STAR(3) > CONCAT(2) > OR(1)
        const std::unordered_map<TokenType, int> precedence = {
            {TokenType::STAR, 3},
            {TokenType::CONCAT, 2},
            {TokenType::OR, 1},
        };
        for (const auto &[type, value]: tokens) {
            switch (type) {
                // 1. 普通字符：直接加入到后缀表达式
                case TokenType::CHAR: {
                    postfix += value;
                    break;
                }
                // 2. 左括号：直接入栈，不参与优先级比较
                case TokenType::LPAREN: {
                    op_stack.push(type);
                    break;
                }
                // 3. 右括号：弹出元素并添加到后缀表达式直到遇到左括号才停止
                case TokenType::RPAREN: {
                    // 直到弹出左括号
                    while (!op_stack.empty() && op_stack.top() != TokenType::LPAREN) {
                        postfix += static_cast<char>(op_stack.top());
                        op_stack.pop();
                    }
                    if (op_stack.empty()) {
                        throw std::invalid_argument("Mismatched parenthese (missing '(')");
                    }
                    // 弹出左括号
                    op_stack.pop();
                    break;
                }
                // 4. 普通运算符：(STAR/CONCAT/OR)：按优先级弹出
                case TokenType::STAR:
                case TokenType::CONCAT:
                case TokenType::OR: {
                    // 弹出优先级 >= 当前运算符
                    while (!op_stack.empty() && op_stack.top() != TokenType::LPAREN &&
                           precedence.at(op_stack.top()) >= precedence.at(type)) {
                        postfix += static_cast<char>(op_stack.top());
                        op_stack.pop();
                    }
                    op_stack.push(type);
                    break;
                }
            }
        }
        // 弹出剩余运算符
        while (!op_stack.empty()) {
            if (op_stack.top() == TokenType::LPAREN) {
                throw std::invalid_argument("Mismatched parenthese (missing ')')");
            }
            postfix += static_cast<char>(op_stack.top());
            op_stack.pop();
        }
        return postfix;
    }

    // NFA 构建：后缀表达式 -> NFA
    std::unique_ptr<NFA> build_nfa(const std::string &postfix) {
        std::stack<std::unique_ptr<NFA> > nfa_stack;
        for (const char c: postfix) {
            switch (static_cast<TokenType>(c)) {
                // 闭包处理
                case TokenType::STAR: {
                    if (nfa_stack.empty()) {
                        throw std::invalid_argument("Invalid postfix: STAR needs 1 operand!");
                    }
                    auto a = std::move(nfa_stack.top());
                    nfa_stack.pop();
                    nfa_stack.push(create_kleene_closure(std::move(a)));
                    break;
                }
                case TokenType::CONCAT: {
                    if (nfa_stack.size() < 2) {
                        throw std::invalid_argument("Invalid postfix: CONCAT needs 2 operands!");
                    }
                    auto b = std::move(nfa_stack.top());
                    nfa_stack.pop();
                    auto a = std::move(nfa_stack.top());
                    nfa_stack.pop();
                    nfa_stack.push(create_concatenate_nfa(std::move(a), std::move(b)));
                    break;
                }
                case TokenType::OR: {
                    if (nfa_stack.size() < 2) {
                        throw std::invalid_argument("Invalid postfix: OR needs 2 operands!");
                    }
                    auto b = std::move(nfa_stack.top());
                    nfa_stack.pop();
                    auto a = std::move(nfa_stack.top());
                    nfa_stack.pop();
                    nfa_stack.push(create_alternative_nfa(std::move(a), std::move(b)));
                    break;
                }
                default: {
                    nfa_stack.push(create_char_nfa(c));
                    break;
                }
            }
        }
        if (nfa_stack.size() != 1) {
            throw std::invalid_argument("Invalid postfix: mismatched operands/operators");
        }
        return std::move(nfa_stack.top());
    }

    // DFA 构建: NFA -> DFA ，NFA 所有权转移到 DFA
    std::unique_ptr<DFA> build_dfa(std::unique_ptr<NFA> nfa) {
        return nullptr;
    }

    // DFA 最小化：原始 DFA -> 最小 DFA
    std::unique_ptr<DFA> minimize_dfa(const DFA &original_dfa) {
        return nullptr;
    }

    // 匹配：最小 DFA + 输入字符串 -> 是否完全匹配
    bool match(const DFA &dfa, std::string_view input) {
        return false;
    }

    // 工具函数：重置状态 ID 计数器，用于多次测试/构建
    void reset_state_counters() {}
}
