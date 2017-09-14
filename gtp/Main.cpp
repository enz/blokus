#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <unistd.h>
#include <sys/times.h>
#include "board.h"
#include "search.h"
#include "opening.h"
#include "libboardgame_gtp/Engine.h"

using namespace std;
using libboardgame_gtp::Arguments;
using libboardgame_gtp::Failure;
using libboardgame_gtp::Response;

//-----------------------------------------------------------------------------

class Engine
        : public libboardgame_gtp::Engine
{
public:
    Engine();

    void cmd_clear_board(const Arguments&);
    void cmd_cputime(Response&);
    void cmd_final_score(Response&);
    void cmd_genmove(const Arguments&, Response&);
    void cmd_play(const Arguments&);
    void cmd_set_game(const Arguments&);
    void cmd_showboard(Response&);

private:
    Board m_bd;

    // 13729 is the number of piece placements in Duo and therefore an upper
    // limit to the number of legal moves.
    unique_ptr<Move[]> m_moves = make_unique<Move[]>(13729);

    int get_color_arg(const Arguments& args, unsigned i) const;
};

Engine::Engine()
{
    add("clear_board", &Engine::cmd_clear_board);
    add("cputime", &Engine::cmd_cputime);
    add("final_score", &Engine::cmd_final_score);
    add("genmove", &Engine::cmd_genmove);
    add("play", &Engine::cmd_play);
    add("set_game", &Engine::cmd_set_game);
    add("showboard", &Engine::cmd_showboard);
}

void Engine::cmd_clear_board(const Arguments& args)
{
    args.check_empty();
    m_bd = Board();
}

void Engine::cmd_cputime(Response& response)
{
    static auto ticks_per_second = double(sysconf(_SC_CLK_TCK));
    struct tms buf;
    if (times(&buf) == clock_t(-1))
        return;
    clock_t clock_ticks =
            buf.tms_utime + buf.tms_stime + buf.tms_cutime + buf.tms_cstime;
    response << (double(clock_ticks) / ticks_per_second);
}

void Engine::cmd_final_score(Response& response)
{
    int score = m_bd.violet_score() - m_bd.orange_score();
    if (score > 0)
        response << "B+" << score;
    else if (score < 0)
        response << "W+" << -score;
    else
        response << "0";
}

void Engine::cmd_genmove(const Arguments& args, Response& response)
{
    args.check_size(1);
    int color = get_color_arg(args, 0);
    if ((color == 0 && ! m_bd.is_violet())
            || (color != 0 && m_bd.is_violet()))
        m_bd.do_pass();

    // Generate move as in com_move() in hm5move.cpp using the parameters
    // for the  highest level in hm5move_post.js
    int max_depth = 10;
    int time_ms = 10000;
    Move mv = opening_move(&m_bd);
    if (mv == INVALID_MOVE)
    {
        SearchResult r;
        if (m_bd.turn() < 25)
            r = search_negascout(&m_bd, max_depth, time_ms / 2, time_ms);
        else if (m_bd.turn() < 27)
            r = wld(&m_bd, 1000);
        else
            r = perfect(&m_bd);
        mv = r.first;
    }

    m_bd.do_move(mv);

    if (mv.is_pass())
    {
        response << "pass";
        return;
    }
    auto& rot = block_set[mv.block_id()]->rotations[mv.direction()];
    int px = mv.x() + rot.offset_x;
    int py = mv.y() + rot.offset_y;
    auto& piece = *rot.piece;
    for (int i = 0; i < piece.size; ++i)
    {
        int x = px + piece.coords[i].x;
        int y = py + piece.coords[i].y;
        response << static_cast<char>('a' + x) << (Board::YSIZE - y);
        if (i < piece.size - 1)
            response << ',';
    }
}

void Engine::cmd_play(const Arguments& args)
{
    args.check_size(2);
    int color = get_color_arg(args, 0);
    if ((color == 0 && ! m_bd.is_violet())
            || (color != 0 && m_bd.is_violet()))
        m_bd.do_pass();

    string move_string = args.get_tolower(1);
    vector<pair<int, int>> coords;
    string::size_type pos = 0;
    while (pos < move_string.size())
    {
        auto next_pos = move_string.find(',', pos);
        string::size_type len;
        if (next_pos != string::npos)
        {
            len = next_pos - pos;
            ++next_pos;
        }
        else
        {
            next_pos = move_string.size();
            len = next_pos - pos;
        }
        auto s = move_string.substr(pos, len);
        if (s.empty())
            throw Failure("invalid move string");
        int x = s[0] - 'a';
        if (x < 0 || x >= Board::XSIZE)
            throw Failure("invalid move string");
        try
        {
            int y = Board::YSIZE - stoi(s.substr(1));
            if (y < 0 || y >= Board::YSIZE)
                throw Failure("invalid move string");
            coords.push_back(make_pair(x, y));
            pos = next_pos;
        }
        catch (const logic_error&)
        {
            throw Failure("invalid move string");
        }
    }

    auto moves = m_moves.get();
    int nu_moves = m_bd.movables(moves, false);
    for (int i = 0; i < nu_moves; ++i)
    {
        Move mv = moves[i];
        const auto& rot = block_set[mv.block_id()]->rotations[mv.direction()];
        auto& piece = *rot.piece;
        if (piece.size != static_cast<int>(coords.size()))
            continue;
        int px = mv.x() + rot.offset_x;
        int py = mv.y() + rot.offset_y;
        bool all_coords_found = true;
        for (int i = 0; i < piece.size; ++i)
        {
            int x = px + piece.coords[i].x;
            int y = py + piece.coords[i].y;
            auto c = make_pair(x, y);
            if (find(coords.begin(), coords.end(), c) == coords.end())
            {
                all_coords_found = false;
                break;
            }
        }
        if (all_coords_found)
        {
            if (! m_bd.is_valid_move(mv))
                throw Failure("illegal move");
            m_bd.do_move(mv);
            return;
        }
    }
    throw Failure("invalid move string");
}

void Engine::cmd_set_game(const Arguments& args)
{
    if (args.get_line() != "Blokus Duo")
        throw Failure("unsupported game");
}

void Engine::cmd_showboard(Response& response)
{
    response << '\n';
    for (int y = Board::YSIZE - 1; y >= 0; --y)
    {
        response << fixed << setw(2) << (y + 1) << ' ';
        for (int x = 0; x < Board::XSIZE; ++x)
        {
            auto s = m_bd.at(x, Board::YSIZE - y - 1);
            if ((s & 0x04) != 0)
                response << 'X';
            else if ((s & 0x40) != 0)
                response << 'O';
            else if ((x == Board::START1X
                      && y == Board::YSIZE - Board::START1Y - 1)
                     || (x == Board::START2X
                         && y == Board::YSIZE - Board::START2Y - 1))
                response << '+';
            else
                response << '.';
            response << ' ';
        }
        response << '\n';
    }
    response << "   ";
    for (int x = 0; x < Board::XSIZE; ++x)
        response << static_cast<char>('A' + x) << ' ';
    response << '\n';
}

int Engine::get_color_arg(const Arguments& args, unsigned i) const
{
    auto s = args.get_tolower(i);
    if (s == "b")
        return 0;
    if (s == "w")
        return 1;
    throw Failure("invalid color argument '" + s + "'");
}

//-----------------------------------------------------------------------------

int main(int, char**)
{
    // Don't print anything to stdout, which would interfere with the GTP
    // stream
    quiet = true; // search.h

    Engine engine;
    engine.exec_main_loop(cin, cout);
    return 0;
}

//-----------------------------------------------------------------------------
