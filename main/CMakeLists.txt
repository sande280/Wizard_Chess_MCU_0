# Run Python script to generate BoardStates.hh from Board_States/*.txt files
execute_process(
    COMMAND ${PYTHON} ${CMAKE_CURRENT_SOURCE_DIR}/convert_board_states.py
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    RESULT_VARIABLE SCRIPT_RESULT
)

if(NOT SCRIPT_RESULT EQUAL 0)
    message(WARNING "convert_board_states.py failed, but continuing build")
endif()

idf_component_register(SRCS "ChessAI.cc" "PathAnalyzer.cc" "BoardStateTracker.cc" "SimpleAI.cc" "esp_chess_main.cc"
                            "ChessBoard.cc"
                            "ChessPiece.cc"
                            "BishopPiece.cc"
                            "KingPiece.cc"
                            "KnightPiece.cc"
                            "PawnPiece.cc"
                            "QueenPiece.cc"
                            "RookPiece.cc"
                       PRIV_REQUIRES driver esp_system
                       INCLUDE_DIRS "."
                       REQUIRES freertos)
