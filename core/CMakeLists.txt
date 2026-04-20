add_library(chess_core
    position.h
    types.h
    bitboards.h
    move.h
    attacks.h
    movegen.h
    position.cpp
    attacks.cpp
    movegen.cpp
)

target_include_directories(chess_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(chess_core PUBLIC cxx_std_20)
