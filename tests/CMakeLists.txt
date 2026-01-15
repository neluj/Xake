add_executable(chess_tests
    test_fen.cpp
)

target_link_libraries(chess_tests PRIVATE chess_core Qt6::Test)

add_test(NAME chess_tests COMMAND chess_tests)
