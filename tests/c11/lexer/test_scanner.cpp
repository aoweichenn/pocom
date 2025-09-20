//
// Created by aowei on 2025 9月 21.
//

#include <gtest/gtest.h>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <c11/lexer/scanner.hpp>
using namespace c11;

#include <gtest/gtest.h>
#include "pocom_scanner.hpp"
#include <sstream>
#include <vector>
#include <string>

using namespace c11;

// 辅助函数：将Token列表转换为字符串表示，便于测试比较
std::string tokens_to_string(const std::vector<Token> &tokens) {
    std::stringstream ss;
    for (const auto &token: tokens) {
        ss << "[" << Scanner::token_type_to_string(token.type) << ":" << token.value << "] ";
    }
    return ss.str();
}

// 辅助函数：将错误列表转换为字符串表示
std::string errors_to_string(const std::vector<ScanError> &errors) {
    std::stringstream ss;
    for (const auto &error: errors) {
        ss << "[" << Scanner::error_type_to_string(error.type) << " at ("
                << error.line << "," << error.column << "): " << error.message << "] ";
    }
    return ss.str();
}

// 测试关键字识别
TEST(ScannerTest, KeywordRecognition) {
    Scanner scanner;
    std::string code = "int float double char void if else for while do return break continue "
            "struct union enum typedef const volatile static extern auto register";

    ScanResult result = scanner.scan(code);

    EXPECT_EQ(result.errors.size(), 0);
    EXPECT_EQ(result.tokens.size(), 21);

    // 所有关键字都应该被识别为TOK_KEYWORD
    for (const auto &token: result.tokens) {
        EXPECT_EQ(token.type, TokenType::TOK_KEYWORD);
    }
}

// 测试标识符识别
TEST(ScannerTest, IdentifierRecognition) {
    Scanner scanner;
    std::string code = "var1 _var2 Var3 var_with_underscores 123invalid";

    ScanResult result = scanner.scan(code);

    EXPECT_EQ(result.errors.size(), 0);
    EXPECT_EQ(result.tokens.size(), 5);

    // 前4个是合法标识符
    for (int i = 0; i < 4; ++i) {
        EXPECT_EQ(result.tokens[i].type, TokenType::TOK_IDENTIFIER);
    }

    // 最后一个会被拆分为整数和标识符
    EXPECT_EQ(result.tokens[4].type, TokenType::TOK_INTEGER);
    EXPECT_EQ(result.tokens[4].value, "123");
}

// 测试整数常量识别
TEST(ScannerTest, IntegerRecognition) {
    Scanner scanner;
    std::string code = "123 0x1A 0XfF 0123 0 123u 456U 789l 0L 123ul";

    ScanResult result = scanner.scan(code);

    EXPECT_EQ(result.errors.size(), 0);
    EXPECT_EQ(result.tokens.size(), 10);

    for (const auto &token: result.tokens) {
        EXPECT_EQ(token.type, TokenType::TOK_INTEGER);
    }
}

// 测试浮点数常量识别
TEST(ScannerTest, FloatRecognition) {
    Scanner scanner;
    std::string code = "123.45 67. .89 10e5 20E-3 30.45f 50.67F 70.89l 90.0L";

    ScanResult result = scanner.scan(code);

    EXPECT_EQ(result.errors.size(), 0);
    EXPECT_EQ(result.tokens.size(), 9);

    for (const auto &token: result.tokens) {
        EXPECT_EQ(token.type, TokenType::TOK_FLOAT);
    }
}

// 测试字符常量识别
TEST(ScannerTest, CharRecognition) {
    Scanner scanner;
    std::string code = "'a' '\\n' '\\t' '\\'' '\\0' '\\x2A'";

    ScanResult result = scanner.scan(code);

    EXPECT_EQ(result.errors.size(), 0);
    EXPECT_EQ(result.tokens.size(), 6);

    for (const auto &token: result.tokens) {
        EXPECT_EQ(token.type, TokenType::TOK_CHAR);
    }
}

// 测试字符串常量识别
TEST(ScannerTest, StringRecognition) {
    Scanner scanner;
    std::string code = "\"hello\" \"line1\\nline2\" \"with \\\"quote\\\"\" \"empty\"";

    ScanResult result = scanner.scan(code);

    EXPECT_EQ(result.errors.size(), 0);
    EXPECT_EQ(result.tokens.size(), 4);

    for (const auto &token: result.tokens) {
        EXPECT_EQ(token.type, TokenType::TOK_STRING);
    }
}

// 测试运算符识别
TEST(ScannerTest, OperatorRecognition) {
    Scanner scanner;
    std::string code = "+ - * / % ++ -- = += -= *= /= %= == != < > <= >= && || ! & | ^ ~ << >>";

    ScanResult result = scanner.scan(code);

    EXPECT_EQ(result.errors.size(), 0);
    EXPECT_EQ(result.tokens.size(), 28);

    for (const auto &token: result.tokens) {
        EXPECT_EQ(token.type, TokenType::TOK_OPERATOR);
    }
}

// 测试标点符号识别
TEST(ScannerTest, PunctuatorRecognition) {
    Scanner scanner;
    std::string code = "() {} [] , ; : . ? -> .* ->* #";

    ScanResult result = scanner.scan(code);

    EXPECT_EQ(result.errors.size(), 0);
    EXPECT_EQ(result.tokens.size(), 15);

    for (const auto &token: result.tokens) {
        EXPECT_EQ(token.type, TokenType::TOK_PUNCTUATOR);
    }
}

// 测试注释识别
TEST(ScannerTest, CommentRecognition) {
    Scanner scanner;
    std::string code = "// 单行注释\n"
            "int a; /* 多行\n"
            "注释 */ float b; /* 单行多行注释 */";

    ScanResult result = scanner.scan(code);

    EXPECT_EQ(result.errors.size(), 0);
    // 应该识别出3个注释和2个关键字、1个标识符、1个分号
    EXPECT_EQ(result.tokens.size(), 7);

    // 检查注释
    EXPECT_EQ(result.tokens[0].type, TokenType::TOK_COMMENT);
    EXPECT_EQ(result.tokens[3].type, TokenType::TOK_COMMENT);
    EXPECT_EQ(result.tokens[6].type, TokenType::TOK_COMMENT);
}

// 测试未闭合字符串错误
TEST(ScannerTest, IncompleteStringError) {
    Scanner scanner;
    std::string code = "\"未闭合的字符串";

    ScanResult result = scanner.scan(code);

    EXPECT_EQ(result.errors.size(), 1);
    EXPECT_EQ(result.errors[0].type, ErrorType::INCOMPLETE_STRING);
}

// 测试未闭合字符错误
TEST(ScannerTest, IncompleteCharError) {
    Scanner scanner;
    std::string code = "'未闭合的字符";

    ScanResult result = scanner.scan(code);

    EXPECT_EQ(result.errors.size(), 1);
    EXPECT_EQ(result.errors[0].type, ErrorType::INCOMPLETE_CHAR);
}

// 测试非法转义序列错误
TEST(ScannerTest, IllegalEscapeError) {
    Scanner scanner;
    std::string code = "\"非法转义: \\a \\z\" '\\q'";

    ScanResult result = scanner.scan(code);

    // 应该有两个非法转义错误
    EXPECT_EQ(result.errors.size(), 2);
    EXPECT_EQ(result.errors[0].type, ErrorType::ILLEGAL_ESCAPE);
    EXPECT_EQ(result.errors[1].type, ErrorType::ILLEGAL_ESCAPE);
}

// 测试未闭合多行注释错误
TEST(ScannerTest, IncompleteCommentError) {
    Scanner scanner;
    std::string code = "int x; /* 未闭合的多行注释\n"
            "这是第二行";

    ScanResult result = scanner.scan(code);

    EXPECT_EQ(result.errors.size(), 1);
    EXPECT_EQ(result.errors[0].type, ErrorType::INCOMPLETE_COMMENT);
}

// 测试无效字符错误
TEST(ScannerTest, InvalidCharacterError) {
    Scanner scanner;
    std::string code = "int x = 10; $ %";

    ScanResult result = scanner.scan(code);

    // $和%是无效字符
    EXPECT_EQ(result.errors.size(), 2);
    EXPECT_EQ(result.errors[0].type, ErrorType::INVALID_CHARACTER);
    EXPECT_EQ(result.errors[1].type, ErrorType::INVALID_CHARACTER);
}

// 测试复杂代码片段
TEST(ScannerTest, ComplexCodeFragment) {
    Scanner scanner;
    std::string code = "/* 计算斐波那契数列 */\n"
            "int fibonacci(int n) {\n"
            "    if (n <= 1)\n"
            "        return n;\n"
            "    return fibonacci(n-1) + fibonacci(n-2);\n"
            "}\n"
            "\n"
            "int main() {\n"
            "    int num = 10; // 计算前10个斐波那契数\n"
            "    printf(\"斐波那契数列前%d项: \", num);\n"
            "    for (int i = 0; i < num; i++) {\n"
            "        printf(\"%d \", fibonacci(i));\n"
            "    }\n"
            "    return 0;\n"
            "}";

    ScanResult result = scanner.scan(code);

    EXPECT_EQ(result.errors.size(), 0);
    // 检查是否识别了基本结构
    bool found_main = false;
    bool found_fibonacci = false;
    for (const auto &token: result.tokens) {
        if (token.type == TokenType::TOK_IDENTIFIER && token.value == "main") {
            found_main = true;
        }
        if (token.type == TokenType::TOK_IDENTIFIER && token.value == "fibonacci") {
            found_fibonacci = true;
        }
    }
    EXPECT_TRUE(found_main);
    EXPECT_TRUE(found_fibonacci);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}


