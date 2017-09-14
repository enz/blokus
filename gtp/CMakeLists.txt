cmake_minimum_required(VERSION 2.8)
project(irori-blokus)

set(CMAKE_CXX_STANDARD 14)
find_package(Ruby QUIET)
if(NOT RUBY_EXECUTABLE)
    message(FATAL_ERROR "Ruby exectable not found.")
endif()

set(BACKEND "${CMAKE_CURRENT_SOURCE_DIR}/../backend")

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/piece.cpp
    COMMAND ${RUBY_EXECUTABLE} ${BACKEND}/piece.rb > ${CMAKE_CURRENT_BINARY_DIR}/piece.cpp
    DEPENDS ${BACKEND}/piece.rb
    )

include_directories(${BACKEND})

add_executable(irori-blokus
    Main.cpp
    ${BACKEND}/board.cpp
    ${BACKEND}/opening.cpp
    ${BACKEND}/search.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/piece.cpp
    )

target_link_libraries(irori-blokus boardgame_gtp)

add_subdirectory(libboardgame_gtp)
