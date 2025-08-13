#pragma once
#include <chrono>
#include <thread>

#include "../Models/Project_path.h"
#include "Board.h"
#include "Config.h"
#include "Hand.h"
#include "Logic.h"

class Game
{
public:
    Game() : board(config("WindowSize", "Width"), config("WindowSize", "Hight")), hand(&board), logic(&board, &config)
    {
        ofstream fout(project_path + "log.txt", ios_base::trunc);
        fout.close();
    }

    // to start checkers
    int play()
    {
        auto start = chrono::steady_clock::now();
        // проверка на повтор игры
        if (is_replay)
        {
            logic = Logic(&board, &config);
            config.reload();
            board.redraw();
        }
        else
        {
            board.start_draw();
        }
        is_replay = false;

        int turn_num = -1;
        bool is_quit = false;
        const int Max_turns = config("Game", "MaxNumTurns"); // основной игровой цикл
        while (++turn_num < Max_turns)
        {
            beat_series = 0;
            // поиск возможных ходов для текущего игрока
            logic.find_turns(turn_num % 2);
            // если нет ходов - игра окончена
            if (logic.turns.empty())
                break;
            // установка уровня сложности бота
            logic.Max_depth = config("Bot", string((turn_num % 2) ? "Black" : "White") + string("BotLevel"));
            // проверка кто ходит - человек или бот
            if (!config("Bot", string("Is") + string((turn_num % 2) ? "Black" : "White") + string("Bot")))
            {
                // ход человека
                auto resp = player_turn(turn_num % 2);
                if (resp == Response::QUIT)
                {
                    is_quit = true;
                    break;
                }
                else if (resp == Response::REPLAY)
                {
                    is_replay = true;
                    break;
                }
                else if (resp == Response::BACK)
                {
                    // откат хода
                    if (config("Bot", string("Is") + string((1 - turn_num % 2) ? "Black" : "White") + string("Bot")) &&
                        !beat_series && board.history_mtx.size() > 2)
                    {
                        board.rollback();
                        --turn_num;
                    }
                    if (!beat_series)
                        --turn_num;

                    board.rollback();
                    --turn_num;
                    beat_series = 0;
                }
            }
            else
                // ход бота
                bot_turn(turn_num % 2);
        }
        // запись времени игры в лог
        auto end = chrono::steady_clock::now();
        ofstream fout(project_path + "log.txt", ios_base::app);
        fout << "Game time: " << (int)chrono::duration<double, milli>(end - start).count() << " millisec\n";
        fout.close();

        // проверка на повтор игры
        if (is_replay)
            return play();
        if (is_quit)
            return 0;
        // определение результата игры
        int res = 2;
        if (turn_num == Max_turns)
        {
            res = 0; // ничья
        }
        else if (turn_num % 2)
        {
            res = 1; // победа белых
        }
        // показ результата и ожидание действий игрока
        board.show_final(res);
        auto resp = hand.wait();
        if (resp == Response::REPLAY)
        {
            is_replay = true;
            return play();
        }
        return res;
    }

private:
    // обработка хода бота
    void bot_turn(const bool color)
    {
        auto start = chrono::steady_clock::now();

        auto delay_ms = config("Bot", "BotDelayMS");
        // создание отдельного потока для задержки
        thread th(SDL_Delay, delay_ms);
        // поиск лучших ходов для бота
        auto turns = logic.find_best_turns(color);
        th.join();
        bool is_first = true;
        // выполнение найденных ходов
        for (auto turn : turns)
        {
            // задержка между ходами в серии (кроме первого)
            if (!is_first)
            {
                SDL_Delay(delay_ms);
            }
            is_first = false;
            // увеличение счетчика серии боя при съедании
            beat_series += (turn.xb != -1);
            board.move_piece(turn, beat_series);
        }

        // запись времени хода бота в лог
        auto end = chrono::steady_clock::now();
        ofstream fout(project_path + "log.txt", ios_base::app);
        fout << "Bot turn time: " << (int)chrono::duration<double, milli>(end - start).count() << " millisec\n";
        fout.close();
    }

    // обработка хода игрока
    Response player_turn(const bool color)
    {
        // подготовка списка возможных ходов для подсветки
        vector<pair<POS_T, POS_T>> cells;
        for (auto turn : logic.turns)
        {
            cells.emplace_back(turn.x, turn.y);
        }
        board.highlight_cells(cells);
        move_pos pos = {-1, -1, -1, -1};
        POS_T x = -1, y = -1;
        // попытка сделать первый ход
        while (true)
        {
            auto resp = hand.get_cell();
            if (get<0>(resp) != Response::CELL)
                return get<0>(resp);
            pair<POS_T, POS_T> cell{get<1>(resp), get<2>(resp)};

            // проверка корректности выбранной клетки
            bool is_correct = false;
            for (auto turn : logic.turns)
            {
                if (turn.x == cell.first && turn.y == cell.second)
                {
                    is_correct = true;
                    break;
                }
                // проверка полного хода (начальная + конечная позиция)
                if (turn == move_pos{x, y, cell.first, cell.second})
                {
                    pos = turn;
                    break;
                }
            }
            if (pos.x != -1)
                break;
            if (!is_correct)
            {
                // сброс выделения при неверном выборе
                if (x != -1)
                {
                    board.clear_active();
                    board.clear_highlight();
                    board.highlight_cells(cells);
                }
                x = -1;
                y = -1;
                continue;
            }
            // запоминание выбранной фигуры и подсветка возможных ходов
            x = cell.first;
            y = cell.second;
            board.clear_highlight();
            board.set_active(x, y);
            vector<pair<POS_T, POS_T>> cells2;
            for (auto turn : logic.turns)
            {
                if (turn.x == x && turn.y == y)
                {
                    cells2.emplace_back(turn.x2, turn.y2);
                }
            }
            board.highlight_cells(cells2);
        }
        // выполнение хода
        board.clear_highlight();
        board.clear_active();
        board.move_piece(pos, pos.xb != -1);
        if (pos.xb == -1)
            return Response::OK;
        // продолжение боя пока возможно
        beat_series = 1;
        while (true)
        {
            logic.find_turns(pos.x2, pos.y2);
            if (!logic.have_beats)
                break;

            // подсветка возможных продолжений боя
            vector<pair<POS_T, POS_T>> cells;
            for (auto turn : logic.turns)
            {
                cells.emplace_back(turn.x2, turn.y2);
            }
            board.highlight_cells(cells);
            board.set_active(pos.x2, pos.y2);
            // выбор следующего хода в серии
            while (true)
            {
                auto resp = hand.get_cell();
                if (get<0>(resp) != Response::CELL)
                    return get<0>(resp);
                pair<POS_T, POS_T> cell{get<1>(resp), get<2>(resp)};

                bool is_correct = false;
                for (auto turn : logic.turns)
                {
                    if (turn.x2 == cell.first && turn.y2 == cell.second)
                    {
                        is_correct = true;
                        pos = turn;
                        break;
                    }
                }
                if (!is_correct)
                    continue;

                // выполнение продолжения боя
                board.clear_highlight();
                board.clear_active();
                beat_series += 1;
                board.move_piece(pos, beat_series);
                break;
            }
        }

        return Response::OK;
    }

private:
    Config config;
    Board board;
    Hand hand;
    Logic logic;
    int beat_series;
    bool is_replay = false;
};
