cmake_minimum_required(VERSION 3.16)
project(NewsTransformer LANGUAGES CXX)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_GLIBCXX_DEBUG")
    set(CMAKE_DEBUG_POSTFIX d)
    message(STATUS "Setting libCXX debug flag")
endif()

find_package(Threads REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Core Network Sql)

file(GLOB SOURCES "*.cpp")
add_executable(NewsTransformer ${SOURCES})

set_target_properties(NewsTransformer
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

target_link_libraries(NewsTransformer PRIVATE
    Threads::Threads
    Qt6::Core
    Qt6::Network
    Qt6::Sql
)

if(MSVC)
    target_compile_options(NewsTransformer PRIVATE /W3)
else()
    target_compile_options(NewsTransformer PRIVATE -Wall -Wextra -pedantic -Wno-unused-parameter)
endif()

install(TARGETS NewsTransformer DESTINATION bin)
