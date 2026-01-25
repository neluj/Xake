add_library(chess_core
    fen.cpp
    fen.h
    position.cpp
    position.h
    types.h
)

target_include_directories(chess_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(chess_core PUBLIC cxx_std_20)
