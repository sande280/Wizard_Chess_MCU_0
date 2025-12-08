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

bool board_state[BOARD_WIDTH][BOARD_HEIGHT];

// --- Helper Functions ---

bool isValid(int x, int y) {
    return (x >= 0 && x < BOARD_WIDTH && y >= 0 && y < BOARD_HEIGHT);
}

bool isPopulated(int x, int y) {
    return switches->isPopulated(x,y);
}

// Find a parking spot for obstacles
Point findParkingBuff(Point target, Point exclude) {
    for (int dist = 1; dist < BOARD_WIDTH; dist++) {
        for (int dx = -dist; dx <= dist; dx++) {
            for (int dy = -dist; dy <= dist; dy++) {
                if (abs(dx) != dist && abs(dy) != dist) continue;
                int nx = target.x + dx;
                int ny = target.y + dy;
                if (isValid(nx, ny)) {
                    if (!isPopulated(nx, ny) && (nx != exclude.x || ny != exclude.y)) {
                        return {nx, ny};
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
std::vector<Point> calculatePath(Point start, Point end) {
    int dist[BOARD_WIDTH][BOARD_HEIGHT];
    Point parent[BOARD_WIDTH][BOARD_HEIGHT];
    
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

void movePieceSmart(int startX, int startY, int endX, int endY, ChessBoard* board) {
    Point start = {startX, startY};
    Point end = {endX, endY};

    //Update Board State
    for(int i = 0; i < 8; i++)
    {
        for(int j = 0; j < BOARD_HEIGHT; j++)
        {
            ChessPiece* piece = board->getPiece(i, j);
            board_state[i][j] = (piece != nullptr);
        }
    }

    std::vector<Point> path = calculatePath(start, end);
    if (path.empty()) return;

    Point currentPos = start;

    bool hasRestorationJob = false;
    Point restoreSource = {-1, -1}; 
    Point restoreDest = {-1, -1};   

    static bool pendingRestore = false;
    static Point pendingSrc, pendingDst;

    for (size_t i = 0; i < path.size(); i++) {
        Point nextStep = path[i];
        
        // --- 1. Optimization Loop: Merge Multiple Moves ---
        // Look ahead as long as the direction is the same and path is safe
        while (i + 1 < path.size()) {
            Point nextNext = path[i+1];
            int dx1 = nextStep.x - currentPos.x;
            int dy1 = nextStep.y - currentPos.y;
            // Note: Since we might have already merged, 'dx1' represents the TOTAL vector so far.
            // We need to compare the DIRECTION of the next single step with our current vector.
            // Since path[i] -> path[i+1] is always a single step, we check THAT vector.
            
            int stepDx = nextNext.x - nextStep.x;
            int stepDy = nextNext.y - nextStep.y;

            // To verify same direction, we can normalize the vectors or check ratios.
            // Since we are on a grid, simply checking signs or equality of unit vectors works.
            // However, our logic relies on combining steps. 
            // A simpler check: Is the vector from (current -> nextNext) a straight line?
            // i.e., is (nextNext.x - current.x) purely x or purely y, or purely diagonal?
            
            bool sameDir = false;
            int totalDx = nextNext.x - currentPos.x;
            int totalDy = nextNext.y - currentPos.y;
            
            // Check Straight Horizontal
            if (totalDy == 0 && totalDx != 0) sameDir = true;
            // Check Straight Vertical
            else if (totalDx == 0 && totalDy != 0) sameDir = true;
            // Check Perfect Diagonal
            else if (abs(totalDx) == abs(totalDy)) sameDir = true;

            // IF same direction AND the intermediate square (nextStep) is empty...
            // Note: For diagonals, we also need to ensure the corners of the intermediate step are clear!
            // But calculatePath penalizes bad diagonals, so we assume path is decent.
            // We strictly check if the square we are skipping is empty.
            if (sameDir && !isPopulated(nextStep.x, nextStep.y)) {
                // If diagonal, check the corners of the skip we just made
                if (abs(totalDx) == abs(totalDy)) {
                     // We just extended a diagonal. We need to verify the NEW sub-segment is clear.
                     // Specifically, check the corners for the step (nextStep -> nextNext).
                     // This is handled by isDiagonalClear check in pathfinding, but let's be safe:
                     if (!isDiagonalClear(nextStep, nextNext)) break; // Stop merging if squeezed
                }

                // Merge success: Update target and increment index
                nextStep = nextNext;
                i++; 
            } else {
                break; // Stop merging
            }
        }

        // --- 2. Diagonal Safety Check ---
        bool isDiagonal = (abs(currentPos.x - nextStep.x) >= 1 && abs(currentPos.y - nextStep.y) >= 1);
        Point diagCorner = {-1, -1};

        if (isDiagonal) {
            // For a long diagonal merge, we might cross multiple squares.
            // plan_move is straight line. A diagonal move (0,0 -> 3,3) is a straight line in physical space.
            // We just need to make sure we didn't optimize through a "squeeze".
            // The optimization loop above checks `isDiagonalClear` for each sub-step, so we should be good.
        }

        // --- 3. Obstacle Clearing ---
        if (isPopulated(nextStep.x, nextStep.y) && nextStep != end) {
            Point parkingSpot = findParkingBuff(nextStep, currentPos);
            
            plan_move(nextStep.x, nextStep.y, parkingSpot.x, parkingSpot.y, true);
            
            board_state[nextStep.x][nextStep.y] = false;
            board_state[parkingSpot.x][parkingSpot.y] = true;

            hasRestorationJob = true;
            restoreSource = parkingSpot;
            restoreDest = nextStep;
        }

        // --- 4. Execute Move ---
        plan_move(currentPos.x, currentPos.y, nextStep.x, nextStep.y, true);
        
        board_state[currentPos.x][currentPos.y] = false;
        board_state[nextStep.x][nextStep.y] = true;

        // --- 5. Restore Logic ---
        if (pendingRestore && nextStep != pendingDst) {
            plan_move(pendingSrc.x, pendingSrc.y, pendingDst.x, pendingDst.y, true);
            board_state[pendingSrc.x][pendingSrc.y] = false;
            board_state[pendingDst.x][pendingDst.y] = true;
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
         board_state[pendingSrc.x][pendingSrc.y] = false;
         board_state[pendingDst.x][pendingDst.y] = true;
         pendingRestore = false;
    }
}