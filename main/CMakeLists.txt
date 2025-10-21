idf_component_register(SRCS "SimpleAI.cc" "ChessAI.cc" "esp_chess_main.cc"
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
