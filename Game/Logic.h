#pragma once
#include <random>
#include <vector>

#include "../Models/Move.h"
#include "Board.h"
#include "Config.h"

const int INF = 1e9;

class Logic
{
public:
    Logic(Board *board, Config *config) : board(board), config(config)
    {
        rand_eng = std::default_random_engine(
            !((*config)("Bot", "NoRandom")) ? unsigned(time(0)) : 0);
        scoring_mode = (*config)("Bot", "BotScoringType");
        optimization = (*config)("Bot", "Optimization");
    }

    // объявление функции поиска лучших ходов
    vector<move_pos> find_best_turns(const bool color);

private:
    // выполнение хода на копии доски
    vector<vector<POS_T>> make_turn(vector<vector<POS_T>> mtx, move_pos turn) const
    {
        // удаление съеденной фигуры
        if (turn.xb != -1)
            mtx[turn.xb][turn.yb] = 0;
        // превращение в дамку при достижении края
        if ((mtx[turn.x][turn.y] == 1 && turn.x2 == 0) || (mtx[turn.x][turn.y] == 2 && turn.x2 == 7))
            mtx[turn.x][turn.y] += 2;
        // перемещение фигуры
        mtx[turn.x2][turn.y2] = mtx[turn.x][turn.y];
        mtx[turn.x][turn.y] = 0;
        return mtx;
    }

    // вычисление оценки позиции для бота
    double calc_score(const vector<vector<POS_T>> &mtx, const bool first_bot_color) const
    {
        double w = 0, wq = 0, b = 0, bq = 0;
        // подсчет количества фигур каждого типа
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                w += (mtx[i][j] == 1);  // белые шашки
                wq += (mtx[i][j] == 3); // белые дамки
                b += (mtx[i][j] == 2);  // черные шашки
                bq += (mtx[i][j] == 4); // черные дамки
                // дополнительные очки за близость к превращению
                if (scoring_mode == "NumberAndPotential")
                {
                    w += 0.05 * (mtx[i][j] == 1) * (7 - i); // белые ближе к верху
                    b += 0.05 * (mtx[i][j] == 2) * (i);     // черные ближе к низу
                }
            }
        }
        // корректировка для цвета бота
        if (!first_bot_color)
        {
            swap(b, w);
            swap(bq, wq);
        }
        // проверка на окончание игры
        if (w + wq == 0)
            return INF; // бот выиграл
        if (b + bq == 0)
            return 0; // бот проиграл
        // расчет итоговой оценки с учетом веса дамок
        int q_coef = 4;
        if (scoring_mode == "NumberAndPotential")
        {
            q_coef = 5;
        }
        return (b + bq * q_coef) / (w + wq * q_coef);
    }

    // объявление функции поиска первого лучшего хода
    double find_first_best_turn(vector<vector<POS_T>> mtx, const bool color, const POS_T x, const POS_T y, size_t state,
                                double alpha = -1);

    // объявление рекурсивной функции поиска лучших ходов
    double find_best_turns_rec(vector<vector<POS_T>> mtx, const bool color, const size_t depth, double alpha = -1,
                               double beta = INF + 1, const POS_T x = -1, const POS_T y = -1);

public:
    // поиск всех возможных ходов для указанного цвета
    void find_turns(const bool color)
    {
        find_turns(color, board->get_board());
    }

    // поиск возможных ходов для фигуры в указанной позиции
    void find_turns(const POS_T x, const POS_T y)
    {
        find_turns(x, y, board->get_board());
    }

private:
    // поиск всех возможных ходов для указанного цвета на данной доске
    void find_turns(const bool color, const vector<vector<POS_T>> &mtx)
    {
        vector<move_pos> res_turns;
        bool have_beats_before = false;
        // перебор всех клеток доски
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                // проверка фигур нужного цвета
                if (mtx[i][j] && mtx[i][j] % 2 != color)
                {
                    find_turns(i, j, mtx);
                    // приоритет ходов с боем
                    if (have_beats && !have_beats_before)
                    {
                        have_beats_before = true;
                        res_turns.clear();
                    }
                    // добавление ходов (только бои или только обычные)
                    if ((have_beats_before && have_beats) || !have_beats_before)
                    {
                        res_turns.insert(res_turns.end(), turns.begin(), turns.end());
                    }
                }
            }
        }
        turns = res_turns;
        // перемешивание ходов для случайности
        shuffle(turns.begin(), turns.end(), rand_eng);
        have_beats = have_beats_before;
    }

    // поиск возможных ходов для фигуры в указанной позиции на данной доске
    void find_turns(const POS_T x, const POS_T y, const vector<vector<POS_T>> &mtx)
    {
        turns.clear();
        have_beats = false;
        POS_T type = mtx[x][y];
        // поиск ходов с боем
        switch (type)
        {
        case 1:
        case 2:
            // проверка боя для обычных шашек
            for (POS_T i = x - 2; i <= x + 2; i += 4)
            {
                for (POS_T j = y - 2; j <= y + 2; j += 4)
                {
                    if (i < 0 || i > 7 || j < 0 || j > 7)
                        continue;
                    POS_T xb = (x + i) / 2, yb = (y + j) / 2;
                    // проверка возможности съесть фигуру
                    if (mtx[i][j] || !mtx[xb][yb] || mtx[xb][yb] % 2 == type % 2)
                        continue;
                    turns.emplace_back(x, y, i, j, xb, yb);
                }
            }
            break;
        default:
            // проверка боя для дамок
            for (POS_T i = -1; i <= 1; i += 2)
            {
                for (POS_T j = -1; j <= 1; j += 2)
                {
                    POS_T xb = -1, yb = -1;
                    // движение по диагонали до препятствия
                    for (POS_T i2 = x + i, j2 = y + j; i2 != 8 && j2 != 8 && i2 != -1 && j2 != -1; i2 += i, j2 += j)
                    {
                        if (mtx[i2][j2])
                        {
                            // остановка при встрече своей фигуры или второй чужой
                            if (mtx[i2][j2] % 2 == type % 2 || (mtx[i2][j2] % 2 != type % 2 && xb != -1))
                            {
                                break;
                            }
                            xb = i2;
                            yb = j2;
                        }
                        // добавление хода с боем
                        if (xb != -1 && xb != i2)
                        {
                            turns.emplace_back(x, y, i2, j2, xb, yb);
                        }
                    }
                }
            }
            break;
        }
        // поиск обычных ходов если нет боев
        if (!turns.empty())
        {
            have_beats = true;
            return;
        }
        switch (type)
        {
        case 1:
        case 2:
            // обычные ходы для шашек
            {
                POS_T i = ((type % 2) ? x - 1 : x + 1);
                for (POS_T j = y - 1; j <= y + 1; j += 2)
                {
                    if (i < 0 || i > 7 || j < 0 || j > 7 || mtx[i][j])
                        continue;
                    turns.emplace_back(x, y, i, j);
                }
                break;
            }
        default:
            // обычные ходы для дамок
            for (POS_T i = -1; i <= 1; i += 2)
            {
                for (POS_T j = -1; j <= 1; j += 2)
                {
                    // движение по диагонали до препятствия
                    for (POS_T i2 = x + i, j2 = y + j; i2 != 8 && j2 != 8 && i2 != -1 && j2 != -1; i2 += i, j2 += j)
                    {
                        if (mtx[i2][j2])
                            break;
                        turns.emplace_back(x, y, i2, j2);
                    }
                }
            }
            break;
        }
    }

public:
    vector<move_pos> turns; // список возможных ходов
    bool have_beats;        // есть ли ходы с боем
    int Max_depth;          // максимальная глубина поиска для бота

private:
    default_random_engine rand_eng; // генератор случайных чисел
    string scoring_mode;            // режим оценки позиции
    string optimization;            // уровень оптимизации алгоритма
    vector<move_pos> next_move;     // следующие ходы в лучшей последовательности
    vector<int> next_best_state;    // индексы лучших состояний
    Board *board;                   // указатель на игровую доску
    Config *config;                 // указатель на конфигурацию
};
