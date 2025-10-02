idf_component_register(SRCS "hello_world_main.c" "esp_chess_main.cc"
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
