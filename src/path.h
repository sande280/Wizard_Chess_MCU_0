#pragma once
#include <vector>
#include <queue>
#include <cmath>
#include "motionPos.h"
#include "ChessBoard.hh"

// --- Definitions ---
#define BOARD_WIDTH  12
#define BOARD_HEIGHT 8

// --- Costs for Pathfinding ---
#define COST_STRAIGHT       10
#define COST_DIAGONAL       14  // Approx 1.41 * 10
#define PENALTY_OCCUPIED    60  // Cost of moving a piece (equivalent to 6 straight empty moves)
#define PENALTY_SQUEEZE     100 // Penalty for diagonal move between two obstacles

struct Point {
    int x;
    int y;
    
    bool operator==(const Point& other) const {
        return x == other.x && y == other.y;
    }
    bool operator!=(const Point& other) const {
        return !(*this == other);
    }
};

// --- Helper Functions ---

bool isValid(int x, int y) {
    return (x >= 0 && x < BOARD_WIDTH && y >= 0 && y < BOARD_HEIGHT);
}

// Check if a physical position is occupied using board state
bool isOccupied(int x, int y, Student::ChessBoard& board) {
    // Valid chess squares: x = 2-9
    if (x >= 2 && x <= 9) {
        int chessRow = y;
        int chessCol = 9 - x;
        return board.getPiece(chessRow, chessCol) != nullptr;
    }
    // Capture zones (x: 0-1 or 10-11) - check captured piece lists by position
    if (x >= 10) {
        // White captured pieces zone (cols 10-11)
        size_t count = board.getCapturedWhitePieces().size();
        int row = count / 2;
        int col = (count % 2) + 10;
        // Check if this position has a captured piece
        for (size_t i = 0; i < count; i++) {
            int pRow = i / 2;
            int pCol = (i % 2) + 10;
            if (pRow == y && pCol == x) return true;
        }
    } else if (x <= 1) {
        // Black captured pieces zone (cols 0-1)
        size_t count = board.getCapturedBlackPieces().size();
        for (size_t i = 0; i < count; i++) {
            int pRow = i / 2;
            int pCol = i % 2;
            if (pRow == y && pCol == x) return true;
        }
    }
    return false;
}

// Find a parking spot for obstacles
Point findParkingBuff(Point target, Point exclude, Student::ChessBoard& board) {
    for (int dist = 1; dist < BOARD_WIDTH; dist++) {
        for (int dx = -dist; dx <= dist; dx++) {
            for (int dy = -dist; dy <= dist; dy++) {
                if (abs(dx) != dist && abs(dy) != dist) continue;
                int nx = target.x + dx;
                int ny = target.y + dy;
                if (isValid(nx, ny)) {
                    if (!isOccupied(nx, ny, board) && (nx != exclude.x || ny != exclude.y)) {
                        return {nx, ny};
                    }
                }
            }
        }
    }
    return {-1, -1};
}

// Check if a diagonal move is valid and get the clear corner
Point getClearDiagonalPath(Point from, Point to, Student::ChessBoard& board) {
    if (abs(from.x - to.x) == 1 && abs(from.y - to.y) == 1) {
        Point corner1 = {to.x, from.y};
        Point corner2 = {from.x, to.y};

        if (!isOccupied(corner1.x, corner1.y, board)) return corner1;
        if (!isOccupied(corner2.x, corner2.y, board)) return corner2;
    }
    return {-1, -1};
}

// Node struct for Dijkstra
struct Node {
    int x, y;
    int cost;
    
    // Priority Queue needs to pop the SMALLEST cost, so we invert the operator
    bool operator>(const Node& other) const {
        return cost > other.cost;
    }
};

// Weighted Pathfinding (Dijkstra)
std::vector<Point> calculatePath(Point start, Point end, Student::ChessBoard& board) {
    // Distance map initialized to infinity (represented by -1 or max int)
    int dist[BOARD_WIDTH][BOARD_HEIGHT];
    Point parent[BOARD_WIDTH][BOARD_HEIGHT];
    
    for(int i=0; i<BOARD_WIDTH; i++) {
        for(int j=0; j<BOARD_HEIGHT; j++) {
            dist[i][j] = 2000000000; // Infinity
            parent[i][j] = {-1, -1};
        }
    }

    // Min-Priority Queue
    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> pq;

    // Start node setup
    dist[start.x][start.y] = 0;
    pq.push({start.x, start.y, 0});

    // 8 Directions
    int dx[] = {0, 0, 1, -1, 1, 1, -1, -1};
    int dy[] = {1, -1, 0, 0, 1, -1, 1, -1};

    bool found = false;

    while (!pq.empty()) {
        Node curr = pq.top();
        pq.pop();

        // If we found a shorter path to this node already, skip
        if (curr.cost > dist[curr.x][curr.y]) continue;

        if (curr.x == end.x && curr.y == end.y) {
            found = true;
            break; 
        }

        // Explore neighbors
        for (int i = 0; i < 8; i++) {
            int nx = curr.x + dx[i];
            int ny = curr.y + dy[i];

            if (isValid(nx, ny)) {
                
                // --- Calculate Move Cost ---
                int stepCost = (i < 4) ? COST_STRAIGHT : COST_DIAGONAL;
                
                // Penalty for entering an occupied square
                // (Unless it's the destination, which implies a capture/landing)
                // Note: Even for captures, we might want to path AROUND others to get there.
                if (isOccupied(nx, ny, board) && (nx != end.x || ny != end.y)) {
                    stepCost += PENALTY_OCCUPIED;
                }

                // Special Check for Diagonals: The "Squeeze" Factor
                if (i >= 4) { // Diagonal indices
                    bool c1_occ = isOccupied(curr.x, ny, board); // Corner 1
                    bool c2_occ = isOccupied(nx, curr.y, board); // Corner 2

                    // If BOTH corners are occupied, this is a "squeeze" move.
                    // Add huge penalty to discourage repelling.
                    if (c1_occ && c2_occ) {
                        stepCost += PENALTY_SQUEEZE;
                    }
                }

                int newCost = dist[curr.x][curr.y] + stepCost;

                if (newCost < dist[nx][ny]) {
                    dist[nx][ny] = newCost;
                    parent[nx][ny] = {curr.x, curr.y};
                    pq.push({nx, ny, newCost});
                }
            }
        }
    }

    std::vector<Point> path;
    if (!found) return path;

    Point curr = end;
    while (curr != start) {
        path.insert(path.begin(), curr);
        curr = parent[curr.x][curr.y];
    }
    return path;
}

void movePieceSmart(int startX, int startY, int endX, int endY, Student::ChessBoard& board) {
    Point start = {startX, startY};
    Point end = {endX, endY};

    std::vector<Point> path = calculatePath(start, end, board);
    if (path.empty()) return;

    Point currentPos = start;

    bool hasRestorationJob = false;
    Point restoreSource = {-1, -1};
    Point restoreDest = {-1, -1};

    static bool pendingRestore = false;
    static Point pendingSrc, pendingDst;

    for (Point nextStep : path) {

        bool isDiagonal = (abs(currentPos.x - nextStep.x) == 1 && abs(currentPos.y - nextStep.y) == 1);
        Point diagCorner = {-1, -1};

        if (isDiagonal) {
            diagCorner = getClearDiagonalPath(currentPos, nextStep, board);
        }

        // --- Logic Branch ---

        // CASE 1: Diagonal and Clear
        if (isDiagonal && diagCorner.x != -1) {
            plan_move(currentPos.x, currentPos.y, diagCorner.x, diagCorner.y, true);
            plan_move(diagCorner.x, diagCorner.y, nextStep.x, nextStep.y, true);
        }

        // CASE 2: Standard Logic (Straight or Blocked Diagonal)
        else {
            if (isOccupied(nextStep.x, nextStep.y, board) && nextStep != end) {
                Point parkingSpot = findParkingBuff(nextStep, currentPos, board);

                plan_move(nextStep.x, nextStep.y, parkingSpot.x, parkingSpot.y, true);

                hasRestorationJob = true;
                restoreSource = parkingSpot;
                restoreDest = nextStep;
            }

            plan_move(currentPos.x, currentPos.y, nextStep.x, nextStep.y, true);
        }
        
        if (pendingRestore && nextStep != pendingDst) {
            plan_move(pendingSrc.x, pendingSrc.y, pendingDst.x, pendingDst.y, true);
            pendingRestore = false;
        }

        if (hasRestorationJob) {
            pendingRestore = true;
            pendingSrc = restoreSource;
            pendingDst = restoreDest;
            hasRestorationJob = false; 
        }

        currentPos = nextStep;
    }

    if (pendingRestore && currentPos != pendingDst) {
         plan_move(pendingSrc.x, pendingSrc.y, pendingDst.x, pendingDst.y, true);
         pendingRestore = false;
    }
}