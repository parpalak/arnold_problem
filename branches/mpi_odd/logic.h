#ifndef LOGIC_H
#define LOGIC_H

#include <stdint.h>

#define TQ 1600 // Квант времени воркеров в микросекундах
// #define TQ 100000000 // Квант времени воркеров в микросекундах
#define TDUMP 20 // Время срабатывания будильника в диспетчере для автодампа состояния

// #define trace(format, ...) printf (format, __VA_ARGS__)
#define trace(format, ...)

#define n_limit 43
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
