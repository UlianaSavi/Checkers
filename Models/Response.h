#pragma once

enum class Response
{
    OK,     // нормальное выполнение действия
    BACK,   // откат хода назад
    REPLAY, // повтор игры заново
    QUIT,   // выход из игры
    CELL    // выбрана клетка на доске
};
