//
// Created by aowei on 2025 9月 16.
//

#include <algorithm>
#include <iostream>
#include <atomic>
#include <queue>
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

// DFA 构建辅助函数
namespace lexer::regex {
    namespace {
        // TODO: 详细学习这个算法
        // 1. 计算 DFA 的 ε 闭包
        std::unordered_set<NFAState *> epsilon_closure(const std::unordered_set<NFAState *> &states) {
            std::unordered_set<NFAState *> closure = states;
            std::queue<NFAState *> q;
            for (auto *s: states) q.push(s);
            while (!q.empty()) {
                auto *current = q.front();
                q.pop();
                // 遍历所有的 ε 转移
                auto it = current->transitions.find(EPSILON);
                if (it == current->transitions.end()) continue;
                for (auto *next: it->second) {
                    if (!closure.count(next)) {
                        closure.insert(next);
                        q.push(next);
                    }
                }
            }
            return closure;
        }

        // TODO: 详细学习这个算法
        // 2. 计算字符转移
        std::unordered_set<NFAState *> move(const std::unordered_set<NFAState *> &states, char c) {
            std::unordered_set<NFAState *> result;
            for (auto *s: states) {
                auto it = s->transitions.find(c);
                if (it != s->transitions.end()) {
                    for (auto *next: it->second) {
                        result.insert(next);
                    }
                }
            }
            return result;
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
    std::vector<Token> lexer(const std::string_view processed_regex) {
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
    std::unique_ptr<DFA> build_dfa(const std::unique_ptr<NFA> &nfa) {
        auto dfa = std::make_unique<DFA>();
        if (!nfa || !nfa->start) {
            throw std::invalid_argument("Cannot build DFA from invalid NFA!");
        }
        // 1. 初始状态：NFA 起始状态的 ε 闭包
        const auto initial_nfa_states = epsilon_closure({nfa->start});
        // 2. 创建 DFA 初始状态，判断是否为接受状态
        bool is_initial_accept = false;
        for (const auto *s: initial_nfa_states) {
            if (s->is_accept) {
                is_initial_accept = true;
                break;
            }
        }
        auto initial_dfa_state = std::make_unique<DFAState>(is_initial_accept);
        DFAState *initial_dfa_ptr = dfa->add_state(std::move(initial_dfa_state));
        dfa->state_map[initial_nfa_states] = initial_dfa_ptr;
        dfa->start = initial_dfa_ptr;
        // 3. 广度优先处理所有的 DFA 状态
        std::queue<std::unordered_set<NFAState *> > state_queue;
        state_queue.push(initial_nfa_states);
        while (!state_queue.empty()) {
            const auto current_nfs_states = state_queue.front();
            state_queue.pop();
            auto *current_dfa_state = dfa->state_map.at(current_nfs_states);
            // 收集所有的非 ε 转移的字符，去重
            std::unordered_set<char> input_chars;
            for (auto *s: current_nfs_states) {
                for (const auto &[c,_]: s->transitions) {
                    if (c != EPSILON) {
                        input_chars.insert(c);
                    }
                }
            }
            // 处理每个字符的转移
            for (const char c: input_chars) {
                const auto move_result = move(current_nfs_states, c);
                const auto next_nfa_states = epsilon_closure(move_result);
                // 若状态集合不存在，则创建新的 DFA 状态
                if (!dfa->state_map.count(next_nfa_states)) {
                    bool is_accept = false;
                    for (auto *s: next_nfa_states) {
                        if (s->is_accept) {
                            is_accept = true;
                            break;
                        }
                    }
                    auto new_dfa_state = std::make_unique<DFAState>(is_accept);
                    DFAState *new_dfa_ptr = dfa->add_state(std::move(new_dfa_state));
                    dfa->state_map[next_nfa_states] = new_dfa_ptr;
                    state_queue.push(next_nfa_states);
                }
                // 绑定 DFA 转移
                current_dfa_state->transitions[c] = dfa->state_map.at(next_nfa_states);
            }
        }
        return dfa;
    }

    // DFA 最小化：原始 DFA -> 最小 DFA，分割法实现
    std::unique_ptr<DFA> minimize_dfa(const DFA &original_dfa) {
        // 1. 初始分割，接受/非接收状态
        std::vector<std::unordered_set<DFAState *> > partitions;
        std::unordered_set<DFAState *> accept_part, non_accept_part;
        for (const auto &s_ptr: original_dfa.states) {
            auto *s = s_ptr.get();
            (s->is_accept ? accept_part : non_accept_part).insert(s);
        }
        if (!accept_part.empty()) partitions.push_back(accept_part);
        if (!non_accept_part.empty()) partitions.push_back(non_accept_part);
        // 2. 迭代分割直到稳定下来
        bool changed = true;
        while (changed) {
            changed = false;
            std::vector<std::unordered_set<DFAState *> > new_partitions;
            for (const auto &part: partitions) {
                if (part.size() <= 1) {
                    new_partitions.push_back(part);
                    continue;
                }
                // 按照转移特征分组。同一组转移目标属于同一分区
                std::unordered_map<std::string, std::unordered_set<DFAState *> > groups;
                for (auto *s: part) {
                    // 生成转移特征键，字符+目标分区索引，排序保持一致
                    std::vector<char> chars;
                    for (const auto &[c,_]: s->transitions) chars.push_back(c);
                    std::sort(chars.begin(), chars.end());
                    std::string key;
                    for (const char c: chars) {
                        auto *target = s->transitions.at(c);
                        // 查找目标状态所属分区索引
                        for (size_t i = 0; i < partitions.size(); ++i) {
                            if (partitions[i].count(target)) {
                                key += c + std::to_string(i) + ",";
                                break;
                            }
                        }
                    }
                    groups[key].insert(s);
                }
                // 更新分区。若分组变化则标记 changed
                for (const auto &[_,group]: groups) {
                    new_partitions.push_back(group);
                    if (group.size() != part.size()) changed = true;
                }
            }
            partitions = std::move(new_partitions);
        }
        // 3. 构建最小 DFA
        auto min_dfa = std::make_unique<DFA>();
        // 原始状态 -> 最小状态映射
        std::unordered_map<DFAState *, DFAState *> state_map;
        // 创建最小 DFA 状态，取每一个分区的第一个状态为代表
        for (const auto &part: partitions) {
            auto *original_rep = *part.begin();
            // 新状态的接受性与代表一致
            auto new_state = std::make_unique<DFAState>(original_rep->is_accept);
            DFAState *new_state_ptr = min_dfa->add_state(std::move(new_state));
            // 映射分区内所有状态到新状态
            for (auto *s: part) state_map[s] = new_state_ptr;
            // 绑定最小 DFA 的起始状态
            if (original_rep == original_dfa.start) {
                min_dfa->start = new_state_ptr;
            }
        }
        // 构建最小 DFA 的转移，复制代表状态的转移
        for (const auto &part: partitions) {
            auto *original_rep = *part.begin();
            auto *min_state = state_map.at(original_rep);
            for (const auto &[c,original_target]: original_rep->transitions) {
                min_state->transitions[c] = state_map.at(original_target);
            }
        }
        return min_dfa;
    }

    // 匹配：最小 DFA + 输入字符串 -> 是否完全匹配
    bool match(const DFA &dfa, const std::string_view input) {
        auto *current = dfa.start;
        if (!current) return false;
        for (const char c: input) {
            auto it = current->transitions.find(c);
            if (it == current->transitions.end()) {
                // 无匹配，转移失败
                return false;
            }
            current = it->second;
        }
        return current->is_accept;
    }

    // 工具函数：重置状态 ID 计数器，用于多次测试/构建
    void reset_state_counters() {
        nfa_state_id_counter = 0;
        dfa_state_id_counter = 0;
    }
}
