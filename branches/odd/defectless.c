/**
 * Программа осуществляет обход дерева для поиска бездефектных конфигураций.
 * 
 * Применяется оптимизация для нечетного количества прямых, зафиксирована раскраска, начальные генераторы.
 * Черные области могут быть только треугольниками, так как черные генераторы применяются сразу после белых
 * и тем самым не перебираются.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/time.h>
#include <signal.h>

// May vary
#define n1 47
#define n_step1 (n1*(n1-1) / 2)

// #define DEBUG 1

unsigned int n;

unsigned int level = 0;

typedef uint_fast32_t line_num;

typedef struct {
    line_num generator;
} Stat;

Stat stat[n_step1 + 1];

line_num a[n1]; // Текущая перестановка 

unsigned int b_free; // Количество оставшихся генераторов для подбора
int max_s = 0;

// User input
char filename[80] = "";
int full = 0;

struct timeval start;

double last_signal = -10.0;

void handle_signal(int sig) {
    struct timeval end;

    gettimeofday(&end, NULL);
    double time_taken = end.tv_sec + end.tv_usec / 1e6 -
        start.tv_sec - start.tv_usec / 1e6; // in seconds

    printf("\n [%f] level = %d/%d", time_taken, level, b_free);
    printf("\n%d\n", stat[n/2].generator);

    exit(0);


    for (int i = 0; i < level; i++) {
        if (stat[i].generator % 2) {
            continue;
        }
        printf("%.*s", stat[i].generator, "                            ");
        printf(" %d \n", stat[i].generator);
    }

    printf("\n");

    if (time_taken - last_signal < 1) {
        exit(0);
    }

    last_signal = time_taken;
}

void count_gen(int level) {
    struct timeval end;

    int s, i, max_defects1;
    FILE* f;

    s = -1 + (n & 1); // Поправка от избытка во внешней области

    for (i = 0; i < level; i++) {
        s -= (stat[i].generator & 1) * 2 - 1;
    }
    if (s < 0) {
        s = -s;
    }
    if (s < max_s || !full && s == max_s) {
        return;
    }

    max_s = s;
    max_defects1 = ((n * (n + 1) / 2 - 3) - 3 * max_s) / 2;

    gettimeofday(&end, NULL);
    double time_taken = end.tv_sec + end.tv_usec / 1e6 - start.tv_sec - start.tv_usec / 1e6; // in seconds


    if (strcmp(filename, "") == 0) {
        printf(" %f A=%d %d)", time_taken, s, max_defects1);

        for (i = n / 2; i--; ) {
            printf(" %d", i * 2 + 1);
        }

        for (i = 0; i < level; i++) {
            printf(" %d", stat[i].generator);
            if (stat[i].generator == 0) {
                printf(" 1");
            }
            else if (stat[i].generator % 2 == 0) {
                printf(" %d %d", stat[i].generator - 1, stat[i].generator + 1);
            }
        }

        printf("\n");

        // for (int i = 0; i < level; i++) {
        //     // if (stat[i].generator % 2) {
        //     //     continue;
        //     // }
        //     printf("%.*s", stat[i].generator, "||||||||||||||||||||||||||||||||||||||||||||||||||||");
        //     printf("><");
        //     printf("%.*s\n", n - 2 - stat[i].generator, "||||||||||||||||||||||||||||||||||||||||||||||||||||");
        // }

        // printf("\n");


        fflush(NULL);
        fflush(NULL);
        fflush(stdout);
        fflush(stdout);
    }
    else {
        f = fopen(filename, "a");

        fprintf(f, " %d %d)", s, max_defects1);
        for (i = 0; i < level; i++) {
            fprintf(f, " %d", stat[i].generator);
        }

        fprintf(f, "\n");
        fclose(f);
    }
    return;
}

/**
 * Пересекает прямые по генератору
 *
 * @param   int   generator       номер генератора
 * @param   int   cross_direction 1 если косичка переплетается, 0 если распутывается
 *
 * @return  void
 */
void set(unsigned int generator, unsigned int cross_direction) {
    line_num i;

    b_free += -2 * cross_direction + 1; // Корректируем кол-во оставшихся генераторов

    i = a[generator];
    a[generator] = a[generator + 1];
    a[generator + 1] = i;
}

// // Стратегия перебора -2, +2, +4 ...

// int inline should_process(int cur_gen, int prev_gen) {

//     int k = cur_gen < n - 3 && cur_gen <= prev_gen + 32;

//     return k;
// }

// void inline modify_generator(int* cur_gen, int prev_gen) {
//     // Пример: предыдущий генератор 2. Инициализируем 2, сразу вычитаем, получаем 0
//     // Следующим шагом инкрементим до максимума и вычитаем.
//     *cur_gen += 2;

//     if (*cur_gen < 0) {
//         *cur_gen += 2;
//     }

//     if (*cur_gen == prev_gen) {
//         *cur_gen += 2;
//     }
// }

// int inline init_working_gen(int generator) {
//     return generator - 4;
// }

// int inline init_skip_gen(int curr_generator) {
//     return curr_generator + 2; // todo
// }

// Стратегия перебора -2, n-3, n-5 ...
unsigned int inline should_process(line_num *cur_gen, line_num prev_gen) {
    // Пример: если предыдущий генератор 2, заканчивать надо на 4

    if (*cur_gen == INT_FAST8_MAX) {
        // С помощью условия пропускаем значение -2.
        *cur_gen = (prev_gen > 0) ? prev_gen - 2 : n - 3;

        return 1;
    }

    if (*cur_gen + 2 == prev_gen) {
        if (n - 3 <= prev_gen) {
            // Предотвращаем зацикливание. Когда prev_gen на максимуме и равен n-3, переключать на n-3 нельзя
            return 0;
        }
        *cur_gen =  n - 3;
        return 1;
    }

    if (*cur_gen == prev_gen + 2) {
        return 0;
    }

    *cur_gen -= 2;
    return 1;
}

line_num inline init_working_gen(line_num generator) {
    return INT_FAST8_MAX;
}

line_num inline init_skip_gen(line_num curr_generator) {
    return curr_generator + 2;
}

// //Стратегия перебора от большего к меньшему
// int inline should_process(int cur_gen, int prev_gen) {
//     return cur_gen > prev_gen - 2 && cur_gen > 0 ;
// }

// void inline modify_generator(int* cur_gen, int prev_gen) {
//     *cur_gen -= 2;
//     if (*cur_gen == prev_gen) {
//         *cur_gen -= 2;
//     }
// }

// int inline init_working_gen(int generator) {
//     return n - 1;
// }

// int inline init_skip_gen(int curr_generator) {
//     return 0;
// }

// Calculations for defectless configurations
void calc() {
    max_s = 0;

    // Часть оптимизации. Первые генераторы должны образовать (n-1)/2 внешних черных двуугольников.
    for (int i = n / 2; i--; ) {
        set(i * 2 + 1, 1);
    }

    stat[level].generator = init_working_gen(n-5); // завышенное несуществующее значение, будет уменьшаться

    while (1) {
#ifdef DEBUG 
        printf("-->> start lev = %d gen = %d\n", level, stat[level].generator);
#endif

        if (should_process(&stat[level].generator, level > 0 ? stat[level-1].generator : 0)) {

            unsigned int curr_generator = stat[level].generator;

#ifdef DEBUG 
            printf("test lev = %d gen = %d prev = %d b_free = %d\n", level, curr_generator, stat[level-1].generator, b_free);
#endif

            /**
             * Оптимизации.
             * 
             * 1. Генератор должен пересекать только прямые, которые еще не пересекались.
             * Прямые пересекались, если левая прямая имеет больший номер, чем правая.
             * 
             * 2. Последние генераторы должны образовать (n-1)/2 внешних черных двуугольников.
             * Стороны двуугольников - прямые с убывающими номерами.
             * Эта оптимизация - продолжение фиксации начальных генераторов. Применима только для черных генераторов.
             * Например, прямые 0 и 1 могут пересекаться только последним генератором n - 2 в самом конце.
             * В середине их нельзя пересекать. Тут проверяем хотя бы совпадение генератора и прямых.
             * Можно проверить, что это именно последний генератор, но на практике это не дает заметного ускорения.
             * 
             * 3. Черные генераторы исключаются из перебора.
             * Сразу после применения любого белого генератора применяются два соседних черных генератора
             * (для белого генератора 0 только один соседний правый генератор 1).
             * 
             * 4. Генератор 0 применяется только один раз для первой и последней прямой.
             * Он образует оставшийся внешний черный двуугольник, образованный прямыми 0 и n-1.
             * 
             * 5. Если прямая n-1 начала идти справа налево, остальные прямые пересекать смысла нет.
             * Соответствующие генераторы должны уменьшаться подряд и заканчиваться генератором 0.
             * Остальные генераторы применять в этом интервале не нужно.
             */

            if (curr_generator != 0) {
                // Оптимизация 1 и 3.
                if (a[curr_generator - 1] > a[curr_generator + 1]) {
                    continue;
                }

                // Оптимизация 1 и 3.
                if (a[curr_generator] > a[curr_generator + 2]) {
                    continue;
                }

                // Оптимизация 2.
                if (a[curr_generator] + 1 == a[curr_generator + 2] && curr_generator + 1 + a[curr_generator + 2] !=  n-1) {
                    continue;
                }

                // Оптимизация 1.
                if (a[curr_generator] > a[curr_generator + 1]) {
                    continue;
                }

                // Оптимизация 2.
                if (a[curr_generator - 1] + 1 == a[curr_generator + 1] && curr_generator - 1 + a[curr_generator + 1] !=  n-1) {
                    continue;
                }

                // Оптимизация 2. Закомментировано, потому что практика показывает, что никакого ускорения от этих проверок нет.
                // if (a[curr_generator - 1] - 1 == a[curr_generator] && a[curr_generator] + curr_generator == n-1) {
                //     continue;
                // }
                // if (a[curr_generator + 1] - 1 == a[curr_generator + 2] && a[curr_generator + 1] - 1 + curr_generator == n-1) {
                //     continue;
                // }
            }
            else {
                // Оптимизация 4.
                if (a[curr_generator + 1] != n - 1) {
                    continue;
                }
                if (a[curr_generator] != 0) {
                    continue;
                }
                // Здесь нет смысла проверять оптимизацию 2, так как прямая с номером 1 уже пересекла прямую n-1 и прямую 2.
                // Поэтому a[2] != 1, и пересечение a[1] == 0 и a[2] всегда возможно.
            }

            // Оптимизация 5. Закомментирована, так как не дает прироста в эвристике -2, n-3, n-5...
            // if (a[0] != n-1 && a[n-1] != n-1 && a[curr_generator + 1] != n-1) {
            //     continue;
            // }

            set(curr_generator, 1);
            if (curr_generator != 0) {
                set(curr_generator - 1, 1);
            }
            set(curr_generator + 1, 1);

            level++;

            if (!b_free) {
                count_gen(level);

#ifdef DEBUG 
                printf("count-gen\n");
#endif
                // goto up;
                stat[level].generator = init_skip_gen(curr_generator); // перебора не будет, чтобы вернуться
            }
            else {
                stat[level].generator = init_working_gen(curr_generator); // запускаем перебор заново на другом уровне
            }
        }
        else {
            up:
            if (level <= 0) {
                printf("level <= 0\n");
                break;
            }

            level--;

            set(stat[level].generator + 1, 0);
            if (stat[level].generator != 0) {
                set(stat[level].generator - 1, 0);
            }
            set(stat[level].generator, 0);
        }
    }
}

int main(int argc, char** argv) {
    int i;
    char s[80];

    if (argc <= 1) {
        printf("Usage: %s -n N [-o filename] [-full]\n  -n\n	 line count;\n  -o\n	 output file.\n", argv[0]);

        return 0;
    }

    signal(SIGINT, handle_signal);

    for (i = 1; i < argc; i++) {
        strcpy(s, argv[i]);
        if (0 == strcmp("-n", s))
            sscanf(argv[++i], "%d", &n);
        else if (0 == strcmp("-o", s))
            strcpy(filename, argv[++i]);
        else if (0 == strcmp("-full", s))
            full = 1;
        else {
            printf("Invalid parameter: \"%s\".\nUsage: %s -n N [-o filename] [-full]\n  -n\n	 line count;\n  -o\n	 output file.\n", s, argv[0]);

            return 0;
        }
    }

    if (n % 2 == 0) {
        printf("This program is designed for fast search of odd defectless configurations only.\n");

        return 0;
    }

    if (n > n1) {
        printf("This program is compiled to support up to %d lines, %d given.\n", n1, n);

        return 0;
    }

    b_free = n * (n - 1) / 2;

    for (i = 0; i < n; i++) {
        a[i] = i;
    }

    gettimeofday(&start, NULL);
    calc();

    printf("---------->>> Process terminated.\n");

    return 0;
}
