//
// Created by aowei on 2025/9/15.
//

#include <lexer/cases/identifier.hpp>

namespace lexer::cases {
    // 构建变量名专用的 NFA（RE = ^[a-zA-Z_][a-zA-Z0-9_]*$）
    NFA build_identifier_nfa() {
        NFA nfa{};
        // 步骤一：创建四个核心状态，使用 unique_ptr 确保自动管理内存
        auto s0 = std::make_unique<State>(0); // 起始占位符状态（对应 ^ 锚点）
        auto s1 = std::make_unique<State>(1); // 等待首个字符输入的状态
        auto s2 = std::make_unique<State>(2); // 等待后续字符输入的状态
        auto s3 = std::make_unique<State>(3); // 终止占位符状态（对应 $ 锚点）
        // 步骤二：将状态移动到 NFA 容器里面，转移所有权，unique_ptr 只能 move
        nfa.states.push_back(std::move(s0));
        nfa.states.push_back(std::move(s1));
        nfa.states.push_back(std::move(s2));
        nfa.states.push_back(std::move(s3));
        // 步骤三：获取裸指针，方便后续设置转移，不拥有所有权
        State *p0 = nfa.states[0].get();
        State *p1 = nfa.states[1].get();
        State *p2 = nfa.states[2].get();
        State *p3 = nfa.states[3].get();
        // 步骤四：构建 NFA 转移逻辑，对应变量名的 RE 规则
        // 0 -> eps -> 1：跳过起始占位，进入首字母等待
        p0->eps_transitions.insert(p1);
        // 1 -> [a-zA-Z_]->2：首字母合法则进入后续字符循环
        for (char c = 'A'; c <= 'Z'; ++c) { p1->transitions[c].insert(p2); }
        for (char c = 'a'; c <= 'z'; ++c) { p1->transitions[c].insert(p2); }
        p1->transitions['_'].insert(p2);
        // 2->[a-zA-Z0-9_]->2：后续字符合法则进入循环（闭包）
        for (char c = 'A'; c < 'Z'; ++c) { p2->transitions[c].insert(p2); }
        for (char c = 'a'; c < 'z'; ++c) { p2->transitions[c].insert(p2); }
        for (char c = '0'; c < '9'; ++c) { p2->transitions[c].insert(p2); }
        p2->transitions['_'].insert(p2);
        // 2->eps->3：无后续字符表示进入接受状态（匹配结束）
        p2->eps_transitions.insert(p3);
        // 步骤五：设置 NFA 的起始和接受状态
        nfa.start = p0;
        nfa.accept = p3;
        return nfa;
    }
}

namespace lexer::cases {

}