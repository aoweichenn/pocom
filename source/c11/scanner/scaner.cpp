//
// Created by aowei on 2025 9月 20.
//

#include <unordered_map>
#include <unordered_set>
#include <c11/lexer/scanner.hpp>

// 静态变量定义
namespace c11 {
    const std::vector<std::string> Scanner::keywords = {
        "auto", "break", "case", "char", "const", "continue", "default", "do",
        "double", "else", "enum", "extern", "float", "for", "goto", "if",
        "int", "long", "register", "return", "short", "signed", "sizeof", "static",
        "struct", "switch", "typedef", "union", "unsigned", "void", "volatile", "while"
    };

    const std::vector<std::string> Scanner::operators = {
        "->", "++", "--", "<=", ">=", "==", "!=", "&&", "||",
        "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", "<<", ">>",
        "<<=", ">>=", "+", "-", "*", "/", "%", "!", "&", "|", "^",
        "~", "<", ">", "=", ".", "?", ":"
    };

    const std::vector<std::string> Scanner::punctuators = {
        "(", ")", "{", "}", "[", "]", ";", ",", ":", "#", "##"
    };
}

// 匿名数据
namespace c11 {
    namespace {
        // 转义字符集
        std::unordered_set<char> escape_char{'a', 'b', 'f', 'n', 'r', 't', 'v', '"', '\'', '?', '\\'};
        // TokenType 转字符串 map
        std::unordered_map<TokenType, std::string> token_type_string_map{
            {TokenType::TOK_KEYWORD, "KEYWORD"},
            {TokenType::TOK_IDENTIFIER, "IDENTIFIER"},
            {TokenType::TOK_INTEGER, "INTEGER"},
            {TokenType::TOK_FLOAT, "FLOAT"},
            {TokenType::TOK_CHAR, "CHAR"},
            {TokenType::TOK_STRING, "STRING"},
            {TokenType::TOK_OPERATOR, "OPERATOR"},
            {TokenType::TOK_PUNCTUATOR, "PUNCTUATOR"},
            {TokenType::TOK_COMMENT, "COMMENT"},
            {TokenType::TOK_WHITESPACE, "WHITESPACE"},
            {TokenType::TOK_UNKNOWN, "UNKNOWN"}
        };
        // ErrorType 转字符串 map
        std::unordered_map<ErrorType, std::string> error_type_string_map{
            {ErrorType::INCOMPLETE_STRING, "INCOMPLETE_STRING"},
            {ErrorType::INCOMPLETE_CHAR, "INCOMPLETE_CHAR"},
            {ErrorType::ILLEGAL_ESCAPE, "ILLEGAL_ESCAPE"},
            {ErrorType::INVALID_CHARACTER, "INVALID_CHARACTER"},
            {ErrorType::INCOMPLETE_COMMENT, "INCOMPLETE_COMMENT"}
        };
    }
}

// Scanner 类函数和辅助函数
namespace c11 {
    // 构造函数：初始化正则表达式
    Scanner::Scanner() {
        init_patterns();
        // 使用模板函数初始化匹配器（匹配器的顺序就是优先级）
        this->matchers = {
            this->make_matcher<&Scanner::match_comments>("comment"),
            this->make_matcher_static<&Scanner::match_string>("string"),
            this->make_matcher_static<&Scanner::match_char>("char"),
            this->make_matcher<&Scanner::match_float>("float"),
            this->make_matcher<&Scanner::match_integer>("integer"),
            this->make_matcher_static<&Scanner::match_operator>("operator"),
            this->make_matcher_static<&Scanner::match_punctuator>("punctuator"),
            this->make_matcher<&Scanner::match_identifier>("identifier"),
            this->make_matcher<&Scanner::match_whitespace>("whitespace"),
        };
    }

    //初始化正则表达式，精准匹配 C11 语法规则
    void Scanner::init_patterns() {
        // 标识符：字母/下划线开头，后接字母/数字/下划线
        this->regex_patterns[TokenType::TOK_IDENTIFIER] = std::regex("[a-zA-Z_][a-zA-Z0-9_]*");
        // 整数常量：十进制（123）、八进制（0123）、十六进制（0x1a）
        this->regex_patterns[TokenType::TOK_INTEGER] = std::regex(
            "0x[0-9a-fA-F]+|0X[0-9a-fA-F]+" // 十六进制
            "|0[0-7]*"                      // 八进制
            "|[1-9][0-9]*"                  // 十进制
        );
        // 浮点数常量：123.45、.45、123e-5、123.45e+6
        this->regex_patterns[TokenType::TOK_FLOAT] = std::regex(
            "[0-9]+\\.[0-9]*([eE][+-]?[0-9]+)?"
            "|[0-9]*\\.[0-9]+([eE][+-]?[0-9]+)?"
            "|[0-9]+[eE][+-]?[0-9]+"
        );
        // 字符常量：支持转义字符
        this->regex_patterns[TokenType::TOK_CHAR] = std::regex(
            R"('([^'\\]|\\['"?\\abfnrtv]|\\[0-7]{1,3}|\\x[0-9a-fA-F]+)')"
        );
        // 字符串常量：同字符常量，支持多字符
        this->regex_patterns[TokenType::TOK_STRING] = std::regex(
            R"("([^"]|\\["'?\\abfnrtv]|\\[0-7]{1,3}|\\x[0-9a-fA-F]+)*")"
        );
        // 注释：单行（//...）、多行（/*...*/）
        this->regex_patterns[TokenType::TOK_COMMENT] = std::regex(
            R"(//.*|/\*[\S\s]*?\*/)", std::regex_constants::ECMAScript
        );
        // 空白字符：空格、制表符、换行、回车、换页
        this->regex_patterns[TokenType::TOK_WHITESPACE] = std::regex(R"([ \t\n\r\f]+)");
    }

    // 检查是否为关键字
    bool Scanner::is_keyword(const std::string &str) {
        return std::binary_search(keywords.begin(), keywords.end(), str);
    }

    // 检查字符串/字符常量中的非法转义序列
    std::optional<ScanError> Scanner::check_escape_sequences(
        const std::string &literal,
        const size_t start_line,
        const size_t start_column) {
        size_t pos = 0;
        size_t current_column = start_column;
        while (pos < literal.size()) {
            if (literal[pos] == '\\') {
                // 转义序列在末尾
                if (pos + 1 >= literal.size()) {
                    return ScanError(
                        ErrorType::ILLEGAL_ESCAPE,
                        "Incomplete escape sequence (ends with '\\')",
                        start_line, current_column);
                }
                char esc = literal[pos + 1];
                current_column += 2; // 转义字符占两列
                // 1. 合法转义符：\a、\b、\f、\n、\r、\t、\v、'、?、\\。
                if (escape_char.find(esc) != escape_char.end()) {
                    pos += 2;
                    continue;
                }
                // 2. 八进制转义：\0-\777（1-3 位数字，不含8/9）
                if (std::isdigit(esc) && esc != '8' && esc != '9') {
                    int oct_len = 1;
                    // 最多三位八进制数
                    while (pos + 1 + oct_len < literal.size() && std::isdigit(literal[pos + 1 + oct_len]) &&
                           oct_len < 3) {
                        oct_len++;
                    }
                    current_column += (oct_len - 1);
                    pos += 1 + oct_len;
                    continue;
                }
                // 3. 十六进制转义：\x 后必须跟至少 1 位十六进制数
                if (esc == 'x') {
                    if (pos + 2 >= literal.size() || !std::isxdigit(literal[pos + 2])) {
                        return ScanError(
                            ErrorType::ILLEGAL_ESCAPE,
                            "Hex escape sequence missing digits (\\x requirs 1+ hex digits)",
                            start_line,
                            current_column - 1 // 定位到 \x 的 x 处
                        );
                    }
                    // 跳过所有十六进制数
                    int hex_length = 1;
                    while (pos + 2 + hex_length < literal.size() && std::isxdigit(literal[pos + 2 + hex_length])) {
                        hex_length++;
                    }
                    current_column += hex_length;
                    pos += 2 + hex_length;
                    continue;
                }
                // 4. 非法转义字符，例如 \z、\@ 等
                return ScanError(
                    ErrorType::ILLEGAL_ESCAPE,
                    "Illegal escape sequence: \\" + std::string(1, esc),
                    start_line,
                    current_column - 1 // 定位到 \ 处
                );
            }
            // 非转义字符，正常通过
            pos++;
            current_column++;
        }
        return std::nullopt;
    }

    // 处理未闭合的多行注释，正则无法匹配，需要手动扫描
    void Scanner::handle_unclosed_comment(
        const std::string &input,
        size_t &pos, size_t line, size_t column,
        ScanResult &result) {
        const size_t start_pos = pos;
        pos += 2; // 跳过 /*
        while (pos < input.size()) {
            if (input[pos] == '*' && pos + 1 < input.size() && input[pos + 1] == '/') {
                // 找到闭合符：正常生成 COMMENT Token
                std::string comment = input.substr(start_pos, pos + 2 - start_pos);
                result.tokens.emplace_back(TokenType::TOK_COMMENT, comment, line, column);
                pos += 2;
                return;
            }
            // 处理换行
            if (input[pos] == '\n') {
                line++;
                column = 1;
            } else {
                column++;
            }
            pos++;
        }
        // 输入结束时仍未找到 */：记录未闭合注释错误
        std::string partial_comment = input.substr(start_pos);
        result.errors.emplace_back(
            ErrorType::INCOMPLETE_COMMENT,
            "Unclosed multi-line comment (missing '*/')",
            line,
            column
        );
        // 生成部分注释 Token，便于定位
        result.tokens.emplace_back(TokenType::TOK_COMMENT, partial_comment, line, column);
    }

    // TokenType 转字符串
    std::string Scanner::token_type_to_string(const TokenType type) {
        const auto it = token_type_string_map.find(type);
        if (it != token_type_string_map.end()) {
            return it->second;
        }
        return "UNKNOWN";
    }

    // ErrorType 转字符串
    std::string Scanner::error_type_to_string(const ErrorType type) {
        const auto it = error_type_string_map.find(type);
        if (it != error_type_string_map.end()) {
            return it->second;
        }
        return "UNKNOWN";
    }
}

// Scanner 扫描逻辑中的辅助函数，单个扫描函数
namespace c11 {
    // 辅助函数，用于更新位置信息
    void Scanner::update_position(const std::string &matched_string, size_t &line, size_t &column) {
        for (const char c: matched_string) {
            if (c == '\n') {
                line++;
                column = 1;
            } else if (c == '\t') {
                column += 4;
            } else {
                column++;
            }
        }
    }

    // 匹配注释
    bool Scanner::match_comments(const std::string &input, size_t &pos, size_t &line, size_t &column,
                                 ScanResult &result) const {
        const size_t input_length = input.length();
        // 先检查是否是多行注释开头（/*）但未闭合
        if (pos + 1 < input_length && input.substr(pos, 2) == "/*") {
            // 尝试匹配完整注释，使用正则进行匹配
            std::smatch match;
            if (std::regex_search(input.cbegin() + static_cast<long>(pos), input.cend(), match,
                                  this->regex_patterns.at(TokenType::TOK_COMMENT),
                                  std::regex_constants::match_continuous)) {
                std::string comment = match.str();
                size_t start_line = line;
                size_t start_column = column;
                // 更新位置
                update_position(comment, line, column);
                pos += comment.size();
                // 添加 Token
                result.tokens.emplace_back(TokenType::TOK_COMMENT, comment, start_line, start_column);
            } else {
                // 未闭合的多行注释：截取到输入的末尾
                std::string incomplete_comment = input.substr(pos);
                size_t start_line = line, start_column = column;
                // 收集错误
                result.errors.emplace_back(
                    ErrorType::INCOMPLETE_COMMENT,
                    "Unclosed multi-line comment (missing '*/')",
                    start_line,
                    start_column
                );
                // 更新位置到输入末尾
                update_position(incomplete_comment, line, column);
                pos = input_length;
                // 添加 UNKNOWN Token （便于追踪）
                result.tokens.emplace_back(TokenType::TOK_UNKNOWN, incomplete_comment, start_line, start_column);
                return true;
            }
        } else if (pos + 1 < input_length && input.substr(pos, 2) == "//") {
            std::smatch match;
            if (std::regex_search(input.cbegin() + static_cast<long>(pos), input.end(), match,
                                  this->regex_patterns.at(TokenType::TOK_COMMENT),
                                  std::regex_constants::match_continuous)) {
                std::string comment = match.str();
                size_t start_line = line, start_column;
                update_position(comment, line, column);
                pos += comment.size();
                result.tokens.emplace_back(TokenType::TOK_COMMENT, comment, start_line, start_column);
                return true;
            }
        }
        return false;
    }

    // 匹配字符串常量
    bool Scanner::match_string(const std::string &input, size_t &pos, size_t &line, size_t &column,
                               ScanResult &result) {
        const size_t input_length = input.length();
        if (input[pos] != '"') return false;
        size_t start_line = line, start_column = column, end_pos = pos + 1;
        // 找闭合的 " （跳过转义的 "）
        while (end_pos < input_length) {
            if (input[end_pos] == '"' && input[end_pos - 1] != '\\') {
                break; // 找到未转义的闭合的 "
            }
            end_pos++;
        }
        if (end_pos >= input_length) {
            // 未闭合的字符串：截止到输入末尾
            std::string incomplete_string = input.substr(pos);
            std::string message = "Unclosed string literal (missing '\"')";
            result.errors.emplace_back(ErrorType::INCOMPLETE_STRING, message, start_line, start_column);
            update_position(incomplete_string, line, column);
            pos = input_length;
            result.tokens.emplace_back(TokenType::TOK_UNKNOWN, incomplete_string, start_line, start_column);
        } else {
            // 闭合字符串：检查转义错误
            std::string string_literal = input.substr(pos, end_pos - pos + 1);
            if (auto escape_err = check_escape_sequences(string_literal, start_line, start_column)) {
                result.errors.push_back(std::move(*escape_err));
            }
            // 更新位置
            update_position(string_literal, line, column);
            pos = end_pos + 1;
            result.tokens.emplace_back(TokenType::TOK_STRING, string_literal, start_line, start_column);
        }
        return true;
    }

    // 匹配字符常量
    bool Scanner::match_char(const std::string &input, size_t &pos, size_t &line, size_t &column,
                             ScanResult &result) {
        const size_t input_length = input.length();
        if (input[pos] != '\'') return false;
        size_t start_line = line, start_column = column, end_pos = pos + 1;
        // 找到闭合的 ' 跳过转义的 '
        while (end_pos < input_length) {
            if (input[end_pos] == '\'' && input[end_pos - 1] != '\\') {
                break;
            }
            end_pos++;
        }
        if (end_pos >= input_length) {
            // 处理未闭合字符
            std::string incomplete_char = input.substr(pos);
            std::string message = "Unclosed character literal (missing '\'')";
            result.errors.emplace_back(ErrorType::INVALID_CHARACTER, message, start_line, start_column);
            // 更新位置信息
            update_position(incomplete_char, line, column);
            pos = input_length;
            result.tokens.emplace_back(TokenType::TOK_UNKNOWN, incomplete_char, line, column);
        } else {
            // 闭合字符：检查转义 + 长度（c语言字符常量只能有一个字符）
            std::string char_literal = input.substr(pos, end_pos - pos + 1);
            if (auto escape_err = check_escape_sequences(char_literal, start_line, start_column)) {
                result.errors.push_back(std::move(*escape_err));
            }
            // 更新位置信息
            update_position(char_literal, line, column);
            pos = end_pos + 1;
            result.tokens.emplace_back(TokenType::TOK_CHAR, char_literal, start_line, start_column);
        }
        return true;
    }

    // 匹配浮点常量
    bool Scanner::match_float(const std::string &input, size_t &pos, const size_t &line, const size_t &column,
                              ScanResult &result) const {
        std::smatch match;
        if (std::regex_search(input.cbegin() + static_cast<long>(pos), input.cend(), match,
                              this->regex_patterns.at(TokenType::TOK_FLOAT),
                              std::regex_constants::match_continuous
        )) {
            std::string float_value = match.str();
            size_t start_line = line, start_column = column;
            // 确保不和整数冲突（例如："123." 是浮点数，"123" 是整数）
            if (float_value.find('.') != std::string::npos ||
                float_value.find('e') != std::string::npos ||
                float_value.find('E') != std::string::npos) {
                pos += float_value.size();
                result.tokens.emplace_back(TokenType::TOK_FLOAT, float_value, start_line, start_column);
                return true;
            }
        }
        return false;
    }

    // 匹配整数常量，含非法整数检测
    bool Scanner::match_integer(const std::string &input, size_t &pos, size_t &line, size_t &column,
                                ScanResult &result) const {
        std::smatch match;
        if (std::regex_search(input.cbegin() + static_cast<long>(pos), input.cend(), match,
                              this->regex_patterns.at(TokenType::TOK_INTEGER),
                              std::regex_constants::match_continuous)) {
            std::string integer_value = match.str();
            size_t start_line = line, start_column = column;
            bool invalid = false;
            // 检测非法十六进制数，例如：0x1G、0XaH
            if (integer_value.size() >= 2 && integer_value.substr(0, 2) == "0x" ||
                integer_value.substr(0, 2) == "0X") {
                for (size_t i = 2; i < integer_value.size(); ++i) {
                    if (!std::isxdigit(integer_value[i])) {
                        invalid = true;
                    }
                }
            }
            // 检测非法八进制数，例如 08、09
            else if (integer_value[0] == '0' && integer_value.size() > 1) {
                for (size_t i = 1; i < integer_value.size(); ++i) {
                    if (integer_value[i] < '0' || integer_value[i] > '7') {
                        invalid = true;
                        break;
                    }
                }
            }
            // 收集非法整数错误
            if (invalid) {
                std::string message = "Invalid integer literal ('" + integer_value + "')";
                result.errors.emplace_back(ErrorType::INVALID_INTEGER, message, start_line, start_column);
            }
            // 更新位置
            update_position(integer_value, line, column);
            pos += integer_value.size();
            result.tokens.emplace_back(TokenType::TOK_INTEGER, integer_value, start_line, start_column);
            return true;
        }
        return false;
    }

    // 匹配运算符
    bool Scanner::match_operator(const std::string &input, size_t &pos, size_t &line, size_t &column,
                                 ScanResult &result) {
        const size_t input_length = input.length();
        for (const auto &op: operators) {
            if (pos + op.size() > input_length) continue;
            if (input.substr(pos, op.size()) == op) {
                size_t start_line = line, start_column = column;
                update_position(op, line, column);
                pos += op.size();
                result.tokens.emplace_back(TokenType::TOK_OPERATOR, op, start_line, start_column);
                return true;
            }
        }
        return false;
    }

    // 匹配标点符号
    bool Scanner::match_punctuator(const std::string &input, size_t &pos, size_t &line, size_t &column,
                                   ScanResult &result) {
        const size_t input_length = input.length();
        for (const auto &punc: punctuators) {
            if (pos + punc.size() > input_length) continue;
            if (input.substr(pos, punc.size()) == punc) {
                size_t start_line = line, start_column = column;
                update_position(punc, line, column);
                pos += punc.size();
                result.tokens.emplace_back(TokenType::TOK_PUNCTUATOR, punc, start_line, start_column);
                return true;
            }
        }
        return false;
    }

    // 匹配标识符/关键字
    bool Scanner::match_identifier(const std::string &input, size_t &pos, size_t &line, size_t &column,
                                   ScanResult &result) const {
        std::smatch match;
        if (std::regex_search(input.cbegin() + static_cast<long>(pos), input.cend(), match,
                              this->regex_patterns.at(TokenType::TOK_IDENTIFIER),
                              std::regex_constants::match_continuous)) {
            std::string id = match.str();
            size_t start_line = line, start_column = column;
            TokenType type = is_keyword(id) ? TokenType::TOK_KEYWORD : TokenType::TOK_IDENTIFIER;
            update_position(id, line, column);
            pos += id.size();
            result.tokens.emplace_back(type, id, start_line, start_column);
            return true;
        }
        return false;
    }

    // 匹配空白字符
    bool Scanner::match_whitespace(const std::string &input, size_t &pos, size_t &line, size_t &column,
                                   [[maybe_unused]] ScanResult &result) const {
        std::smatch match;
        if (std::regex_search(input.cbegin() + static_cast<long>(pos), input.cend(), match,
                              this->regex_patterns.at(TokenType::TOK_WHITESPACE),
                              std::regex_constants::match_continuous)) {
            const std::string whitespace = match.str();
            // 空白字符不添加 Token，只更新位置
            update_position(whitespace, line, column);
            pos += whitespace.size();
            return true;
        }
        return false;
    }

    // 处理无效字符
    void Scanner::handle_invalid_char(const std::string &input, size_t &pos, size_t &line, size_t &column,
                                      ScanResult &result) {
        const char invalid_char = input[pos];
        std::string char_string(1, invalid_char);
        size_t start_line = line, start_column = column;
        // 收集错误
        std::string message = "Invalid character ('" + char_string + "')";
        result.errors.emplace_back(ErrorType::INVALID_CHARACTER, message, start_line, start_column);
        // 更新位置
        update_position(char_string, line, column);
        pos++;
        // 添加 UNKNOWN Token
        result.tokens.emplace_back(TokenType::TOK_UNKNOWN, char_string, start_line, start_column);
    }
}

// Scaaner 核心逻辑函数
namespace c11 {
    // 核心扫描逻辑：逐个字符处理，手机 Token 和错误
    ScanResult Scanner::scan(const std::string &input) const {
        ScanResult result;
        size_t pos = 0;
        size_t column = 1, line = 1;
        const size_t input_length = input.size();
        // 按照优先级一次调用匹配函数
        while (pos < input_length) {
            bool matched = false;
            // 遍历所有的匹配器，按照优先级尝试匹配
            for (const auto &[func, _]: this->matchers) {
                if (func(input, pos, line, column, result)) {
                    matched = true;
                    break;
                }
            }
            // 当所有的匹配都失败的时候：处理无效字符
            if (!matched) {
                handle_invalid_char(input, pos, line, column, result);
            }
        }
        return result;
    }
}
