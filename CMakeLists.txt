cmake_minimum_required(VERSION 3.4.1)
project(mila)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)

execute_process(COMMAND llvm-config-10 --libs OUTPUT_VARIABLE LIBS)
execute_process(COMMAND llvm-config-10 --system-libs OUTPUT_VARIABLE SYS_LIBS)
execute_process(COMMAND llvm-config-10 --ldflags OUTPUT_VARIABLE LDF)
#message(STATUS "Found LLVM" ${LIBS})

string(STRIP ${LIBS} LIBS)
string(STRIP ${SYS_LIBS} SYS_LIBS)
string(STRIP ${LDF} LDF)

link_libraries(${LIBS} ${SYS_LIBS} ${LDF})

execute_process(COMMAND llvm-config-10 --cxxflags OUTPUT_VARIABLE CMAKE_CXX_FLAGS)
string(STRIP ${CMAKE_CXX_FLAGS} CMAKE_CXX_FLAGS)

add_executable(mila main.cpp Lexer.hpp Lexer.cpp Parser.hpp Parser.cpp ast.hpp ast.cpp)
