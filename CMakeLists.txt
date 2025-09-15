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
)

add_executable(pocom
        main.cpp
        include/lexer/cases/identifier.hpp
        source/lexer/cases/identifier.cpp
        include/lexer/cases/utils.hpp
        source/lexer/cases/utils.cpp
)
target_link_libraries(pocom pocoms)