#ifndef PATH_ANALYZER_HH
#define PATH_ANALYZER_HH

#include "ChessBoard.hh"
#include "Chess.h"
#include <string>
#include <utility>

namespace Student {

enum PathType {
    DIRECT_HORIZONTAL,
    DIRECT_VERTICAL,
    DIAGONAL_DIRECT,
    DIRECT_L_PATH,
    INDIRECT_HORIZONTAL,
    INDIRECT_VERTICAL,
    DIAGONAL_INDIRECT,
    INDIRECT_L_PATH,
    CASTLING_INDIRECT
};

class PathAnalyzer {
public:
    static PathType analyzeMovePath(int fromRow, int fromCol, int toRow, int toCol,
                                    ChessBoard& board, Type pieceType, bool isCastling);

    static std::string pathTypeToString(PathType type);

    static std::pair<int, int> findCaptureZoneDestination(ChessBoard& board, Color capturedColor);

private:
    static bool isPathClear(int fromRow, int fromCol, int toRow, int toCol, ChessBoard& board);

    static bool isKnightLPathClear(int fromRow, int fromCol, int toRow, int toCol, ChessBoard& board);
};

} // namespace Student

#endif // PATH_ANALYZER_HH
