cmake_minimum_required(VERSION 3.20)
project(pocom LANGUAGES CXX
        VERSION 1.0.0
        DESCRIPTION "pocom"
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


include_directories(include)

add_library(pocoms STATIC
        main.cpp
        include/lexer/cases/identifier.hpp
        source/lexer/cases/identifier.cpp
        include/lexer/cases/utils.hpp
        source/lexer/cases/utils.cpp
        include/lexer/regex/engine.hpp
        source/lexer/regex/engine.cpp
        include/c11/lexer/scanner.hpp
        source/c11/scanner/scaner.cpp
)

add_executable(pocom
        main.cpp
        include/lexer/cases/identifier.hpp
        source/lexer/cases/identifier.cpp
        include/lexer/cases/utils.hpp
        source/lexer/cases/utils.cpp
        include/lexer/regex/engine.hpp
        source/lexer/regex/engine.cpp
        include/c11/lexer/scanner.hpp
        source/c11/scanner/scaner.cpp
)
target_link_libraries(pocom pocoms)

# 查找已安装的GTest包
find_package(GTest REQUIRED)
if (GTest_FOUND)
    message(STATUS "Found GTest: ${GTEST_VERSION}")
else ()
    message(FATAL_ERROR "GTest not found. Please install GTest development package.")
endif ()

# 启用测试
enable_testing()

# 添加测试目标
add_executable(pocom_tests
        tests/c11/lexer/test_scanner.cpp
)

# 链接测试库
target_link_libraries(pocom_tests
        pocoms
        GTest::gtest_main
        GTest::gmock_main
)

# 添加包含目录
target_include_directories(pocom_tests PRIVATE
        include
)

# 添加测试到CTest
include(GoogleTest)
gtest_discover_tests(pocom_tests)