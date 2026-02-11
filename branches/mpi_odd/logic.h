#ifndef LOGIC_H
#define LOGIC_H

#include <stdint.h>

#define TQ 1800 // Квант времени воркеров в микросекундах
// #define TQ 700000 // Квант времени воркеров в микросекундах
#define TDUMP 20 // Время срабатывания будильника в диспетчере для автодампа состояния

#define SORTING_ORDER 1 // в естественном порядке, определяемом направлением обхода
#define SORTING_EVALUATION_LOG_LINEAR 2 // на основе оценки с линейной интерполяцией
#define SORTING_EVALUATION_LOG_SQUARE 3 // на основе оценки с квадратичной интерполяцией
#define SORTING_EVALUATION_LOG_AVG 4 // на основе оценки с усреднением
#define SORTING_EVALUATION_AVG 5 // на основе оценки с усреднением без логарифмирования (похоже на максимум)
#define GENERATOR_SORTING SORTING_ORDER

#define INITIALIZATION_ROOT 1 // одна начальная задача
#define INITIALIZATION_FIRST_LEVEL 2 // все начальные задачи для первого уровня
#define INITIALIZATION INITIALIZATION_ROOT

#define MULTIPLET_DECR 1 // x-2, n-3, n-5, ... x+2
#define MULTIPLET_INCR 2 // x-2, x+2, x+4, ... n-3
#define MULTIPLET MULTIPLET_DECR

// Отдаем будущую работу диспетчеру, а сами добьем с текущего места (старая логика).
// Проблема в том, что текущую работу сложно оценить (evaluation).
#define FORK_NEXT 1
// Отдаем текущую работу диспетчеру, а сами прыгаем вперед (новая логика).
// Текущую отданную работу легко оценить, но процесс может быть долго занят прыжками и форками.
// Также существенно растет объем очереди.
#define FORK_CURRENT 2
#define FORK_JOB FORK_NEXT

// #define trace(format, ...) printf (format, __VA_ARGS__)
#define trace(format, ...)

#define n_limit 39
#define n_step_limit (n_limit*(n_limit-1) / 2)

#define plurality ((n_limit-1)/2) // Количество узлов дерева на каждом уровне
#define rearrangement_count 1 // Количество вариантов перестановок для обхода этих узлов

#define max(a,b) ((a) > (b) ? (a) : (b))
#define min(a,b) ((a) < (b) ? (a) : (b))

extern int n, n_step;

extern int triplets[rearrangement_count][plurality]; //= {{-2, n-3, n-5, ... 2}};

typedef uint_fast32_t line_num;

typedef struct {
    line_num generator;
    unsigned long int processed; // количество проходов по уровню с момента предыдущего срабатывания будильника
    int rearrangement;
    int rearr_index;
    int level_count;
} Stat;

extern Stat stat[n_step_limit + 1];

extern line_num a[n_limit]; // Текущая перестановка

#endif // LOGIC_H
