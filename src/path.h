#pragma once
#include <vector>
#include <queue>
#include <cmath>
#include "reed.hpp"
#include "motionPos.h"
#include "ChessBoard.hh"

// --- Definitions ---
#define BOARD_WIDTH  12
#define BOARD_HEIGHT 8

// --- Costs for Pathfinding ---
#define COST_STRAIGHT       10
#define COST_DIAGONAL       14
#define PENALTY_OCCUPIED    500
#define PENALTY_SQUEEZE     1000

using namespace Student;
using namespace std;

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

struct RestorationJob {
    Point source; // Where the piece is currently parked
    Point dest;   // Where it belongs
};

uint8_t board_state[BOARD_WIDTH][BOARD_HEIGHT] = {0};

// --- Helper Functions ---

bool isValid(int x, int y) {
    return (x >= 0 && x < BOARD_WIDTH && y >= 0 && y < BOARD_HEIGHT);
}

bool isPopulated(int x, int y) {
    return board_state[x][y];
}

// Updated findParkingBuff: Now accepts the full path to avoid parking ON the path
Point findParkingBuff(Point target, Point exclude, const std::vector<Point>& futurePath) {
    for (int dist = 1; dist < BOARD_WIDTH; dist++) {
        for (int dx = -dist; dx <= dist; dx++) {
            for (int dy = -dist; dy <= dist; dy++) {
                if (abs(dx) != dist && abs(dy) != dist) continue;
                int nx = target.x + dx;
                int ny = target.y + dy;
                
                if (isValid(nx, ny)) {
                    // 1. Must be physically empty (or the exclusion point)
                    if (!isPopulated(nx, ny) && (nx != exclude.x || ny != exclude.y)) {
                        
                        // 2. NEW: Must NOT be on the future path of the main piece
                        bool onPath = false;
                        for(const auto& p : futurePath) {
                            if (p.x == nx && p.y == ny) {
                                onPath = true;
                                break;
                            }
                        }
                        
                        if (!onPath) return {nx, ny};
                    }
                }
            }
        }
    }
    return {-1, -1};
}

// Check if a diagonal move is truly clear (both destination AND corners)
bool isDiagonalClear(Point from, Point to) {
    if (abs(from.x - to.x) == 1 && abs(from.y - to.y) == 1) {
        Point corner1 = {to.x, from.y};
        Point corner2 = {from.x, to.y};

        if (isPopulated(corner1.x, corner1.y)) return false;
        if (isPopulated(corner2.x, corner2.y)) return false;
        
        if (isPopulated(to.x, to.y)) return false;

        return true; 
    }
    return false;
}

// Node struct for Dijkstra
struct Node {
    int x, y;
    int cost;
    bool operator>(const Node& other) const {
        return cost > other.cost;
    }
};

// Weighted Pathfinding (Dijkstra)
// Using static arrays to avoid stack overflow from recursive calls
std::vector<Point> calculatePath(Point start, Point end) {
    static int dist[BOARD_WIDTH][BOARD_HEIGHT];
    static Point parent[BOARD_WIDTH][BOARD_HEIGHT];
    
    for(int i=0; i<BOARD_WIDTH; i++) {
        for(int j=0; j<BOARD_HEIGHT; j++) {
            dist[i][j] = 2000000000;
            parent[i][j] = {-1, -1};
        }
    }

    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> pq;

    dist[start.x][start.y] = 0;
    pq.push({start.x, start.y, 0});

    int dx[] = {0, 0, 1, -1, 1, 1, -1, -1};
    int dy[] = {1, -1, 0, 0, 1, -1, 1, -1};

    bool found = false;

    while (!pq.empty()) {
        Node curr = pq.top();
        pq.pop();

        if (curr.cost > dist[curr.x][curr.y]) continue;

        if (curr.x == end.x && curr.y == end.y) {
            found = true;
            break; 
        }

        for (int i = 0; i < 8; i++) {
            int nx = curr.x + dx[i];
            int ny = curr.y + dy[i];

            if (isValid(nx, ny)) {
                int stepCost = (i < 4) ? COST_STRAIGHT : COST_DIAGONAL;
                
                // Penalty for entering an occupied square
                if (isPopulated(nx, ny) && (nx != end.x || ny != end.y)) {
                    stepCost += PENALTY_OCCUPIED;
                }

                // Check for Diagonal Squeeze
                if (i >= 4) { 
                    bool c1_occ = isPopulated(curr.x, ny);
                    bool c2_occ = isPopulated(nx, curr.y);
                    if (c1_occ || c2_occ) { 
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

void setupMoveTracking(ChessBoard* board)
{
    for(int i = 0; i < 8; i++)
    {
        for(int j = 0; j < BOARD_HEIGHT; j++)
        {
            ChessPiece* piece = board->getPiece(i, j);
            if(piece != nullptr)
            {
                board_state[9-j][i] = (piece->getColor() == Color::Black) ? 1 : 2;
            }
            ESP_LOGI("PATHFINDING", "Board State [%d][%d]: %d", 9-j, i, board_state[9-j][i] ? 1 : 0);
        }
    }
}

Point findEmptyCapture(int x, int y)
{
    uint8_t row1, row2;
    Point out = {-1, -1};
    if(board_state[x][y] == 1)
    {
        row1 = 0;
        row2 = 1;
    }
    else
    {
        row1 = 11;
        row2 = 10;
    }

    //Loop through to find best side
    if(y < 4)
    {
        for(int i = 0; i < 4; i++)
        {
            if(!board_state[row1][i])
            {
                out.x = row1;
                out.y = i;
                break;
            }
            else if(!board_state[row2][i])
            {
                out.x = row2;
                out.y = i;
                break;
            }
        }
    }
    else
    {
        for(int i = 7; i > 3; i--)
        {
            if(!board_state[row1][i])
            {
                out.x = row1;
                out.y = i;
                break;
            }
            else if(!board_state[row2][i])
            {
                out.x = row2;
                out.y = i;
                break;
            }
        }
    }

    return out;
}

// Maximum recursion depth to prevent stack overflow
#define MAX_PATHFINDING_DEPTH 8

void movePieceSmartInternal(int startX, int startY, int endX, int endY, int depth);

void movePieceSmart(int startX, int startY, int endX, int endY) {
    movePieceSmartInternal(startX, startY, endX, endY, 0);
}

void movePieceSmartInternal(int startX, int startY, int endX, int endY, int depth) {
    // Prevent stack overflow from deep recursion
    if (depth >= MAX_PATHFINDING_DEPTH) {
        ESP_LOGW("PATHFINDING", "Max recursion depth reached, using direct move");
        plan_move(startX, startY, endX, endY, true);
        board_state[endX][endY] = board_state[startX][startY];
        board_state[startX][startY] = 0;
        return;
    }

    Point start = {startX, startY};
    Point end = {endX, endY};

    std::vector<Point> path = calculatePath(start, end);
    if (path.empty()) return;

    for(int i = 0; i < path.size(); i++)
    {
        ESP_LOGI("PATHFINDING", "Path Step %d: (%d, %d)", i, path[i].x, path[i].y);
        ESP_LOGI("PATHFINDING", "Board State at Step %d:", isPopulated(path[i].x, path[i].y) ? 1 : 0);
    }

    Point currentPos = start;
    std::vector<RestorationJob> restorationQueue;

    for (size_t i = 0; i < path.size(); i++) {
        Point nextStep = path[i];
        
        bool populated = false;
        // --- Optimization Loop ---
        while (i + 1 < path.size()) {
            //Check if i is populated
            if (isPopulated(nextStep.x, nextStep.y) && nextStep != end) {
                Point parkingSpot = findParkingBuff(nextStep, currentPos, path);

                movePieceSmartInternal(nextStep.x, nextStep.y, parkingSpot.x, parkingSpot.y, depth + 1);

                restorationQueue.push_back({parkingSpot, nextStep});
            }

            Point nextNext = path[i+1];
            int totalDx = nextNext.x - currentPos.x;
            int totalDy = nextNext.y - currentPos.y;
            bool sameDir = false;
            
            if (totalDy == 0 && totalDx != 0) sameDir = true;
            else if (totalDx == 0 && totalDy != 0) sameDir = true;
            else if (abs(totalDx) == abs(totalDy)) sameDir = true; 
            
            if(sameDir)
            {
                //Check for Diagonal
                if(abs(totalDx) == abs(totalDy))
                {
                    if(!isDiagonalClear(nextStep, nextNext)) break;
                }
                nextStep = nextNext;
                i++;
            }
            else
            {
                break;
            }
        }

        // --- 4. Execute Move ---
        plan_move(currentPos.x, currentPos.y, nextStep.x, nextStep.y, true);
        
        //Update board state
        board_state[nextStep.x][nextStep.y] = board_state[currentPos.x][currentPos.y];
        board_state[currentPos.x][currentPos.y] = 0;

        currentPos = nextStep;
    }

    // --- EXECUTE DEFERRED RESTORATIONS ---
    // Now that the main piece has finished its entire journey, put everything back.
    // We iterate backwards (LIFO) to unwind the moves.
    for (int i = restorationQueue.size() - 1; i >= 0; i--) {
        RestorationJob job = restorationQueue[i];
        movePieceSmartInternal(job.source.x, job.source.y, job.dest.x, job.dest.y, depth + 1);
    }
}