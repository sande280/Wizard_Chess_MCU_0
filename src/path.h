#pragma once
#include <vector>
#include <queue>
#include <cmath>
#include <list>
#include <algorithm>
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

// Global board state (defined in header as requested)
uint8_t board_state[BOARD_WIDTH][BOARD_HEIGHT] = {0};

// --- Helper Functions ---

bool isValid(int x, int y) {
    return (x >= 0 && x < BOARD_WIDTH && y >= 0 && y < BOARD_HEIGHT);
}

bool isPopulated(int x, int y) {
    return board_state[x][y];
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

// Find a parking spot NOT on the path
Point findParkingBuff(Point target, const std::vector<Point>& futurePath) {
    // Search increasingly wide rings (radius 1 to 3)
    for (int dist = 1; dist <= 3; dist++) {
        for (int dx = -dist; dx <= dist; dx++) {
            for (int dy = -dist; dy <= dist; dy++) {
                // Only check the perimeter of the current ring
                if (abs(dx) != dist && abs(dy) != dist) continue;
                
                int nx = target.x + dx;
                int ny = target.y + dy;
                
                if (isValid(nx, ny)) {
                    // 1. Must be physically empty
                    if (!isPopulated(nx, ny)) {
                        
                        // 2. Must NOT be on the future path of the main piece
                        bool onPath = false;
                        for(const auto& p : futurePath) {
                            if (p.x == nx && p.y == ny) {
                                onPath = true;
                                break;
                            }
                        }
                        
                        // 3. Ensure we can actually GET to the parking spot safely
                        // (Simple adjacency check: if diagonal, check corners)
                        bool accessible = true;
                        if (abs(target.x - nx) == 1 && abs(target.y - ny) == 1) {
                             if (!isDiagonalClear(target, {nx, ny})) accessible = false;
                        }
                        
                        if (!onPath && accessible) return {nx, ny};
                    }
                }
            }
        }
    }
    return {-1, -1};
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
    // Static arrays to avoid stack overflow (12*8*4 bytes is small, but safer on heap/static)
    // However, static makes it non-reentrant. Since this is single-threaded task, it is okay.
    // To be perfectly safe against multiple tasks, local variables are better, 
    // but standard stack size should handle 1KB arrays fine.
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
                
                // Penalty for entering an occupied square (unless destination)
                if (isPopulated(nx, ny) && (nx != end.x || ny != end.y)) {
                    stepCost += PENALTY_OCCUPIED;
                }

                // Check for Diagonal Squeeze
                if (i >= 4) { 
                    Point pFrom = {curr.x, curr.y};
                    Point pTo = {nx, ny};
                    if (!isDiagonalClear(pFrom, pTo)) { 
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
        }
    }
}

// Helper to execute a short move safely (Handling Doglegs)
// This replaces the recursive calls
void performSafeMove(Point start, Point end) {
    bool diagonal = (abs(start.x - end.x) == 1 && abs(start.y - end.y) == 1);
    
    if (diagonal) {
        // If diagonal is clear, move direct
        if (isDiagonalClear(start, end)) {
             plan_move(start.x, start.y, end.x, end.y, true);
        } 
        else {
             // Diagonal blocked by corners -> Dogleg
             // Try Corner 1 (Move X then Y)
             Point c1 = {end.x, start.y};
             if (!isPopulated(c1.x, c1.y)) {
                 plan_move(start.x, start.y, c1.x, c1.y, true);
                 plan_move(c1.x, c1.y, end.x, end.y, true);
             } 
             // Try Corner 2 (Move Y then X)
             else {
                 Point c2 = {start.x, end.y};
                 // If c2 is also populated, we have a problem, but physics will force it.
                 // Just take the path least likely to cause damage (straight lines)
                 plan_move(start.x, start.y, c2.x, c2.y, true);
                 plan_move(c2.x, c2.y, end.x, end.y, true);
             }
        }
    } else {
        // Straight move
        plan_move(start.x, start.y, end.x, end.y, true);
    }
}

void movePieceSmart(int startX, int startY, int endX, int endY) {
    Point start = {startX, startY};
    Point end = {endX, endY};

    // Calculate Path
    std::vector<Point> path = calculatePath(start, end);
    if (path.empty()) return;

    for(int i = 0; i < path.size(); i++)
    {
        ESP_LOGI("PATHFINDING", "Path Step %d: (%d, %d) Occupied: %d", i, path[i].x, path[i].y, isPopulated(path[i].x, path[i].y));
    }

    Point currentPos = start;
    std::list<RestorationJob> restorationQueue; // Using list for easy splice/push

    for (size_t i = 0; i < path.size(); i++) {
        Point nextStep = path[i];
        
        // --- Look-Ahead Optimization ---
        while (i + 1 < path.size()) {
            Point nextNext = path[i+1];
            int totalDx = nextNext.x - currentPos.x;
            int totalDy = nextNext.y - currentPos.y;
            bool sameDir = false;
            
            if (totalDy == 0 && totalDx != 0) sameDir = true;
            else if (totalDx == 0 && totalDy != 0) sameDir = true;
            else if (abs(totalDx) == abs(totalDy)) sameDir = true; 
            
            // Only merge if intermediate step is SAFE (no obstacle)
            if (sameDir && !isPopulated(nextStep.x, nextStep.y)) {
                if (abs(totalDx) == abs(totalDy)) {
                    if(!isDiagonalClear(nextStep, nextNext)) break;
                }
                nextStep = nextNext;
                i++;
            }
            else {
                break;
            }
        }

        // --- Obstacle Handling (Iterative, Non-Recursive) ---
        if (isPopulated(nextStep.x, nextStep.y) && nextStep != end) {
            
            Point parkingSpot = findParkingBuff(nextStep, path);
            
            if (parkingSpot.x != -1) {
                // 1. Move Obstacle to Parking (Safe Move)
                performSafeMove(nextStep, parkingSpot);
                
                // 2. Update Map immediately
                board_state[nextStep.x][nextStep.y] = 0;
                board_state[parkingSpot.x][parkingSpot.y] = 1;
                
                // 3. Queue Restoration
                restorationQueue.push_back({parkingSpot, nextStep});
            } else {
                ESP_LOGE("PATHFINDING", "CRITICAL: No parking spot for obstacle at (%d,%d)", nextStep.x, nextStep.y);
            }
        }

        // --- Execute Main Move ---
        plan_move(currentPos.x, currentPos.y, nextStep.x, nextStep.y, true);
        
        // Update Map
        board_state[nextStep.x][nextStep.y] = board_state[currentPos.x][currentPos.y];
        board_state[currentPos.x][currentPos.y] = 0;

        currentPos = nextStep;
    }

    // --- Execute Deferred Restorations (Reverse Order) ---
    // This puts the last moved piece back first, preventing collisions
    while(!restorationQueue.empty()) {
        RestorationJob job = restorationQueue.back();
        restorationQueue.pop_back();
        
        performSafeMove(job.source, job.dest);
        
        // Update map for correctness
        board_state[job.dest.x][job.dest.y] = board_state[job.source.x][job.source.y];
        board_state[job.source.x][job.source.y] = 0;
    }
}