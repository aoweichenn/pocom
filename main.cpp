//
// Created by aowei on 2025/9/15.
//

#include <iostream>
#include <string>
#include <regex>
#include <vector>
#include <fstream>
#include <c11/lexer/scanner.hpp>

std::string read_file_to_string(const std::string &filename) {
    // 使用 C++17 的文件流，以二进制模式打开（避免文本模式下的换行符转换）
    std::ifstream file(filename, std::ios::binary);

    // 检查文件是否成功打开
    if (!file.is_open()) {
        throw std::runtime_error("无法打开文件: " + filename + "（可能文件不存在或权限不足）");
    }

    // 检查文件是否可读
    if (!file.good()) {
        throw std::runtime_error("文件状态异常: " + filename);
    }

    // 使用 stringstream 作为中间缓冲区
    std::stringstream buffer;

    // 将文件内容复制到缓冲区（C++17 支持直接使用 istreambuf_iterator）
    buffer << file.rdbuf();

    // 检查读取过程是否出现错误
    if (file.fail()) {
        throw std::runtime_error("读取文件失败: " + filename);
    }
    // 返回缓冲区内容作为字符串
    return buffer.str();
}

int main() {
    const std::string code = read_file_to_string(R"(/mnt/d/DEMOS/STU/CPP/pocom/codes/main.c)");
    const c11::Scanner s;
    const auto [tokens, errors] = s.scan(code);
    std::cout << tokens.size();
}
