//
// Created by aowei on 2025 9月 20.
//

#ifndef POCOM_SCANNER_HPP
#define POCOM_SCANNER_HPP

#include <map>
#include <optional>
#include <regex>
#include <string>
#include <utility>
#include <vector>
#include <functional>


namespace c11 {
    // 词法错误类型枚举
    enum class ErrorType {
        INCOMPLETE_STRING,  // 未闭合的字符串，例如 "Hello
        INCOMPLETE_CHAR,    // 未闭合的字符
        ILLEGAL_ESCAPE,     // 非法转义序列
        INVALID_INTEGER,    // 无效整数
        INVALID_CHARACTER,  // 无效字符
        INCOMPLETE_COMMENT, // 未闭合多行注释
    };

    // 词法错误信息结构体，包含错误位置和描述
    struct ScanError {
        ErrorType type;      // 错误类型
        std::string message; //错误描述
        size_t line;         //错误行号
        size_t column;       // 错误列号

        ScanError() = delete;

        explicit ScanError(const ErrorType type, std::string message, const size_t line,
                           const size_t column) : type(type),
                                                  message(std::move(message)), line(line), column(column) {}
    };

    // Token 类型枚举
    enum class TokenType {
        TOK_KEYWORD,    // 关键字
        TOK_IDENTIFIER, // 标识符
        TOK_INTEGER,    // 整数常量
        TOK_FLOAT,      // 浮点数常量
        TOK_CHAR,       // 字符常量
        TOK_STRING,     // 字符串常量
        TOK_OPERATOR,   // 运算符
        TOK_PUNCTUATOR, // 标点符号
        TOK_COMMENT,    // 注释
        TOK_UNKNOWN,    // 未知，以被错误处理覆盖
        TOK_WHITESPACE, // 空白符号
    };

    // Token 结构体
    struct Token {
        TokenType type;
        std::string value;
        size_t line;
        size_t column;

        Token() = delete;

        explicit Token(const TokenType type, std::string value, const size_t line, const size_t column) : type(type),
            value(std::move(value)), line(line), column(column) {}
    };

    // 扫描结果封装，Token 列表 + 错误列表
    struct ScanResult {
        std::vector<Token> tokens;     // 正常识别的 tokens
        std::vector<ScanError> errors; // 收集的词法错误
    };

    // Scanner
    class Scanner {
    private:
        static const std::vector<std::string> keywords;    // 关键字
        static const std::vector<std::string> operators;   // 运算符
        static const std::vector<std::string> punctuators; // 标点符号
        // 正则表达式模式，延迟初始化
        std::map<TokenType, std::regex> regex_patterns;

        void init_patterns();
        static bool is_keyword(const std::string &str);
        // 检查字符串/字符常量中的非法转义序列，返回错误信息
        static std::optional<ScanError> check_escape_sequences(const std::string &literal,
                                                               size_t start_line,
                                                               size_t start_column);
        // 处理未闭合的多行注释
        static void handle_unclosed_comment(const std::string &input, size_t &pos, size_t line, size_t column,
                                            ScanResult &result);

    private:
        // 辅助函数，用于更新位置信息，用来处理换行/制表符
        static void update_position(const std::string &matched_string, size_t &line, size_t &column);
        // 匹配注释
        bool match_comments(const std::string &input, size_t &pos, size_t &line, size_t &column,
                            ScanResult &result) const;
        // 匹配字符串常量
        static bool match_string(const std::string &input, size_t &pos, size_t &line, size_t &column,
                                 ScanResult &result);
        // 匹配字符常量
        static bool match_char(const std::string &input, size_t &pos, size_t &line, size_t &column,
                               ScanResult &result);
        // 匹配浮点常量
        bool match_float(const std::string &input, size_t &pos, size_t &line, size_t &column,
                         ScanResult &result) const;
        // 匹配整数常量，含非法整数检测
        bool match_integer(const std::string &input, size_t &pos, size_t &line, size_t &column,
                           ScanResult &result) const;
        // 匹配运算符
        static bool match_operator(const std::string &input, size_t &pos, size_t &line, size_t &column,
                                   ScanResult &result);
        // 匹配标点符号
        static bool match_punctuator(const std::string &input, size_t &pos, size_t &line, size_t &column,
                                     ScanResult &result);
        // 匹配标识符/关键字
        bool match_identifier(const std::string &input, size_t &pos, size_t &line, size_t &column,
                              ScanResult &result) const;
        // 匹配空白字符
        bool match_whitespace(const std::string &input, size_t &pos, size_t &line, size_t &column,
                              ScanResult &result) const;
        // 处理无效字符
        static void handle_invalid_char(const std::string &input, size_t &pos, size_t &line, size_t &column,
                                        ScanResult &result);

    private:
        // 定义匹配函数的签名：接收输入字符串、位置、行号、列号、扫描结果，返回是否匹配成功
        using MatchFunc = std::function<bool(
            const std::string &input, // 输入的源代码字符串
            size_t &pos,              // 当前解析位置（引用，会被更新）
            size_t &line,             // 当前行号（引用，会被更新）
            size_t &column,           // 当前列号（引用，会被更新）
            ScanResult &result        // 扫描结果（引用，用于存储Token和错误）
        )>;

        // 匹配结构体
        struct Matcher {
            MatchFunc func;
            std::string name;
        };

        // 原有的非静态成员函数版本（保持不变）
        template<auto Method>
        Matcher make_matcher(std::string name) {
            return {
                [this](const std::string &input, size_t &pos, size_t &line, size_t &column, ScanResult &result) {
                    // 非静态成员函数需要通过this调用
                    return (this->*Method)(input, pos, line, column, result);
                },
                std::move(name)
            };
        }

        // 新增：静态成员函数版本（无需捕获this）
        template<auto StaticMethod>
        [[nodiscard]] Matcher make_matcher_static(std::string name) const {
            return {
                [](const std::string &input, size_t &pos, size_t &line, size_t &column, ScanResult &result) {
                    // 静态成员函数直接通过函数指针调用（无需this）
                    return StaticMethod(input, pos, line, column, result);
                },
                std::move(name)
            };
        }

        // 存储所有匹配器
        std::vector<Matcher> matchers;

    public:
        Scanner();
        ~Scanner() = default;
        // 禁止拷贝，但是允许移动
        Scanner(const Scanner &) = delete;
        Scanner &operator=(const Scanner &) = delete;
        Scanner(Scanner &&) = default;
        Scanner &operator=(Scanner &&) = default;
        // 核心扫描接口：输入代码，返回 Token + 错误
        [[nodiscard]] ScanResult scan(const std::string &input) const;
        static std::string token_type_to_string(TokenType type);
        static std::string error_type_to_string(ErrorType type);
    };
}


#endif //POCOM_SCANNER_HPP
