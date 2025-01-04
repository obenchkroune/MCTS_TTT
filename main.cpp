#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <limits>
#include <map>
#include <vector>

#define GREEN "\033[1;32m"
#define RED   "\033[1;31m"
#define RESET "\033[0m"

using namespace std;
using hrc = chrono::high_resolution_clock;

enum Player { EMPTY = 0, BOT, OPP };

map<Player, char> PLAYER_CHAR = {{OPP, 'X'}, {BOT, 'O'}, {EMPTY, ' '}};

struct Point {
    int x = -1;
    int y = -1;

    bool operator==(const Point& rhs) const {
        return rhs.x == x && rhs.y == y;
    }
};

struct Game {
    vector<vector<Player>> grid = {
        {EMPTY, EMPTY, EMPTY},
        {EMPTY, EMPTY, EMPTY},
        {EMPTY, EMPTY, EMPTY},
    };
    vector<Point> possibleMoves = {
        {0, 0}, {0, 1}, {0, 2}, {1, 0}, {1, 1}, {1, 2}, {2, 0}, {2, 1}, {2, 2},
    };

    void printGrid() const {
        for (size_t i = 0; i < grid.size(); i++) {
            for (size_t j = 0; j < grid[i].size(); j++) {
                cout << "| " << PLAYER_CHAR[grid[i][j]] << " ";
            }
            cout << "|" << endl;
        }
    }

    bool applyMove(const Point& pos, Player player) {
        auto it = find_if(possibleMoves.begin(), possibleMoves.end(), [&](const Point& p) {
            return p == pos;
        });
        if (it == possibleMoves.end()) return false;
        grid[it->y][it->x] = player;
        possibleMoves.erase(it);
        return true;
    }

    Player getWinner() const {
        for (int i = 0; i < 3; i++) {
            if (grid[i][0] != EMPTY && grid[i][0] == grid[i][1] && grid[i][0] == grid[i][2]) {
                return grid[i][0] == BOT ? BOT : OPP;
            }
            if (grid[0][i] != EMPTY && grid[0][i] == grid[1][i] && grid[0][i] == grid[2][i]) {
                return grid[0][i] == BOT ? BOT : OPP;
            }
        }
        if (grid[0][0] != EMPTY && grid[0][0] == grid[1][1] && grid[0][0] == grid[2][2]) {
            return grid[0][0] == BOT ? BOT : OPP;
        }
        if (grid[0][2] != EMPTY && grid[0][2] == grid[1][1] && grid[0][2] == grid[2][0]) {
            return grid[0][2] == BOT ? BOT : OPP;
        }
        return EMPTY;
    }

    bool isTerminal() const {
        return possibleMoves.empty() || getWinner() != EMPTY;
    }

    void readUserInput() {
        int    x, y;
        string line;

        while (true) {
            cout << "Enter your move (row and column): ";
            getline(cin, line);

            size_t pos = line.find(',');
            if (pos == string::npos) {
                cout << RED "invalid move" RESET << endl;
                continue;
            }
            y = stoi(line.substr(0, pos));
            x = stoi(line.substr(pos + 1));
            if (!this->applyMove({x, y}, OPP)) {
                cout << RED "invalid move" RESET << endl;
                continue;
            }
            break;
        }
    }
};

struct Node {
    Node(const Game& state, const Point& move, Player player, Node* parent)
        : move(move), state(state), player(player), parent(parent) {
    }

    Point        move;
    Game         state;
    Player       player;
    Node*        parent   = nullptr;
    vector<Node> children = {};
    float        wins     = 0;
    int          visits   = 0;

    float getUCB1() const {
        if (parent == nullptr || visits == 0) {
            return numeric_limits<float>::infinity();
        }
        float exploitation = wins / visits;
        if (player == OPP) {
            exploitation = -exploitation;
        }
        float exploration = 1.41 * sqrt(log(parent->visits) / visits);
        return exploitation + exploration;
    }
};

struct MCTS {
    Node root;

    MCTS(const Game& initialState) : root(initialState, {-1, -1}, OPP, nullptr) {
        // root.visits = 1;
    }

    Node* selection() {
        Node* node = &root;
        while (node->children.size() > 0) {
            node = &*max_element(
                node->children.begin(), node->children.end(),
                [&](const Node& a, const Node& b) { return a.getUCB1() < b.getUCB1(); }
            );
        }
        return node;
    }

    Node* expantion(Node* node) {
        if (node->visits == 0 || node->state.isTerminal()) {
            return node;
        }

        Player player = (node->player == OPP ? BOT : OPP);
        for (const auto& possibleMove : node->state.possibleMoves) {
            Node child = Node{node->state, possibleMove, player, node};
            child.state.applyMove(possibleMove, player);
            node->children.push_back(child);
        }
        return &*max_element(
            node->children.begin(), node->children.end(),
            [&](const Node& a, const Node& b) { return a.getUCB1() < b.getUCB1(); }
        );
    }

    float rollout(Node* node) {
        Game   state(node->state);
        Player player = node->player;

        while (!state.isTerminal()) {
            state.applyMove(state.possibleMoves[rand() % state.possibleMoves.size()], player);
            player = (player == OPP ? BOT : OPP);
        }
        Player winner = state.getWinner();
        if (winner == OPP) return -1.0f;
        if (winner == BOT) return 1.0f;
        return 0.0f;
    }

    void backprob(Node* node, float wins) {
        while (node != nullptr) {
            node->visits += 1;
            node->wins += wins;
            node = node->parent;
        }
    }

    bool stillHasTime(const hrc::time_point& begin, int maximum_time) const {
        auto end = hrc::now();
        return chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() < maximum_time;
    }

    // max time in `ms`
    Point getBestMove(int maximum_time = 50) {
        int  simulations = 0;
        auto begin       = hrc::now();
        while (stillHasTime(begin, maximum_time)) {
            Node* node = expantion(selection());
            backprob(node, rollout(node));
            simulations++;
        }
        cout << "made " << simulations << " simulations in "
             << chrono::duration_cast<chrono::milliseconds>(hrc::now() - begin).count() << " ms"
             << endl;
        return max_element(
                   root.children.begin(), root.children.end(),
                   [&](const Node& a, const Node& b) { return a.visits < b.visits; }
        )->move;
    }
};

int main() {
    Game game;

    while (!game.isTerminal()) {
        game.printGrid();
        game.readUserInput();
        MCTS  mcts(game);
        Point move = mcts.getBestMove();
        game.applyMove(move, BOT);
    }

    game.printGrid();
    Player winner = game.getWinner();
    if (winner == EMPTY) {
        cout << "Draw!" << endl;
        return 0;
    }
    cout << GREEN "Player " << PLAYER_CHAR[winner] << " Won!" RESET << endl;
}