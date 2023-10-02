#include <mpi.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/time.h> 
#include <signal.h>

#define max(a,b) ((a) > (b) ? (a) : (b))
#define min(a,b) ((a) < (b) ? (a) : (b))

// #define trace(format, ...) printf (format, __VA_ARGS__)
#define trace(format, ...)

#define n1 43
#define n_step1 (n1*(n1-1) / 2)

#define plurality (n1-1)/2 // Количество узлов дерева на каждом уровне
#define rearrangement_count 1 // Количество вариантов перестановок для обхода этих узлов

#define TQ 1 // Квант времени воркеров
#define TDUMP 20 // Квант времени воркеров
#define TAG 0

unsigned int n, n_step;

int triplets[rearrangement_count][plurality]; //= {{-2, n-3, n-5, ... 2}};

typedef int line_num;

typedef struct {
	line_num generator;
	line_num prev_polygon_cnt;
	int processed; // why?
	int rearrangement;
	int rearr_index;
    unsigned long int iterations;
} Stat;

Stat stat[n_step1 + 1];

line_num a[n1]; // Текущая перестановка 
line_num d[n1 + 1]; // Количество боковых вершин в текущих многоугольниках вокруг прямых

unsigned int polygon_stat[100];

int max_s;

unsigned int b_free; // Количество оставшихся генераторов для подбора (меняется???)

// User input
char filename[80] = "";
int full = 0;
int show_stat = 0;

struct timeval start;

// MPI
char NODE_NAME[64];
int was_alarm = 0;
int get_timings = 0; 

enum {BUSY, FORKED, FINISHED, QUIT};

typedef struct {
	int level; // Текущий уровень
	int min_level; // Уровень, до которого нужно подняться в этом джобе. Этот уровень ближе к корню, чем текущий уровень.
	int w_level; // Что это?
	int status;
	int rearrangement[n_step1];
	int rearr_index[n_step1];
	int max_s;
	unsigned long int iterations; // Количество предшествующих итераций главного цикла

	int d_search_time; 
	int w_run_time; 
	int w_idle_time; 
	int network_time; 
} Message;

// Message stack
Message **messages = NULL;
int msg_reserve, msg_count;

struct { 
	// Worker's timings 
	int w_run_time; 
	int w_idle_time; 

	// Dispatcher 
	int d_push_msg_time, push_count; 
	int d_run_time; 
	int d_idle_time; 

	// Network's timings 
	int network_time; 
} timings; 

void init_messages_stack() {
	msg_reserve = 1000;
	msg_count = 0;
	timings.push_count = 0;
	messages = (Message**) malloc(msg_reserve*sizeof(Message*));
}

int count_compare;

int msg_compare (const void * a, const void * b) { 
	Message *ma, *mb;
	int *p1, *p2, *endp;

	ma = * (Message **) a;
	mb = * (Message **) b;

	p1 = ma->rearr_index;
	p2 = mb->rearr_index;
	endp = p1 + min(ma->level, mb->level); 

	for (; !(*p2 - *p1) && p1 <= endp; ++p1, ++p2); 

	count_compare++;

	return *p2 - *p1; 
} 

void msg_sort() {
	qsort(messages, msg_count, sizeof(Message *), msg_compare);
}

void push_msg_back(Message *msg) {
	++timings.push_count;
	if (++msg_count > msg_reserve)
		messages = (Message**) realloc(messages, (msg_reserve*=2)*sizeof(Message*));
	messages[msg_count - 1] = msg;
	count_compare = 0;
	msg_sort();
	printf("------->  %d in stack [%d times compared while sorting]  <-------\n", msg_count, count_compare);
}

Message *pop_msg() {
	if (msg_count) {
		return messages[--msg_count];
	}
	return NULL;
}

// Workers state
int* wrk_state;
int wrk_count;

void init_worker_state(int cnt) {
	int i;
	wrk_count = cnt;
	wrk_state = (int *) malloc(wrk_count * sizeof(int));
	for (i = 0; i < wrk_count; ++i) {
		wrk_state[i] = FINISHED;
	}
}

int get_worker(int status) {
	int i;
	for (i = 0; i < wrk_count; ++i) {
		if (wrk_state[i] == status) {
			return i + 1;
		}
	}
	return -1;
}

int set_wrk_state(int i, int state) {
	wrk_state[i-1] = state;
}

void send_timings_signal (int s) { 
	get_timings = 1; 
}

void tmr (int s) {
	was_alarm = 1;
}

void send_message_to_dispatcher(Message *msg) { 
	struct timeval t1, t2; 
	int send_time; 

	msg->w_idle_time = timings.w_idle_time; 
	msg->w_run_time = timings.w_run_time; 
	msg->network_time = timings.network_time; 

	gettimeofday(&t1, NULL); 
	MPI_Send((void *)msg, sizeof(Message)/sizeof(int), MPI_INT, 0, TAG, MPI_COMM_WORLD); 
	gettimeofday(&t2, NULL); 

	send_time =  (t2.tv_sec - t1.tv_sec)*1000 + (t2.tv_usec - t1.tv_usec)/1000; 
	timings.w_idle_time += send_time; 
	timings.network_time += send_time; 
} 

void count_gen (int level, unsigned long int iterations) {
	struct timeval end;
	int i;
	FILE *f;

	gettimeofday(&end, NULL);
    double time_taken = end.tv_sec + end.tv_usec / 1e6 - start.tv_sec - start.tv_usec / 1e6; // in seconds

    /**
     * В конфигурации имеется level четных генераторов. Каждый четный генератор порождает
     * белую область. С ним также ассоциированы два нечетных генератора, которые порождают
     * две черные области. Но с генератором 0 ассоциирован только один черный генератор.
     *
     * Кроме того, есть (n-1)/2 начальных черных генераторов, которые порождают
     * неучтенные черные области.
     *
     * Внешние области чередующегося цвета в количестве n + 1, которые пересекаются
     * в начальном положении сканирующей прямой и которые тоже не учтены,
     * вклада в разность не дают.
     */

	int s = n_step - 1 + (n - 1) / 2;

	max_s = s;

	if (strcmp(filename, "") == 0) {
		f = stdout;
        fprintf(f, "%s [%fs] A=%d", NODE_NAME, time_taken, s);
	} else {
		f = fopen(filename, "a");
        fprintf(f, " [%fs] A=%d", time_taken, s);
	}

    fprintf(f, " i=%e)", (double)iterations);

    for (i = n / 2; i--; ) {
        fprintf(f, " %d", i * 2 + 1);
    }

    for (i=1; i <= level+1; i++) {
        // Белый генератор
        fprintf(f, " %d", (int)stat[i].generator);
        // Ассоциированные черные генераторы
        if (stat[i].generator == 0) {
            fprintf(f, " 1");
        }
        else if (stat[i].generator % 2 == 0) {
            fprintf(f, " %d %d", (int)stat[i].generator - 1, (int)stat[i].generator + 1);
        }
    }

	fprintf(f, "\n");

	if (strcmp(filename, "") == 0) {
		fflush(NULL);
		fflush(NULL);
		fflush(stdout);
		fflush(stdout);
	} else {
		fclose(f);
	}
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

/**
 * Применение белого генератора и двух ассоциированных (соседних) черных.
 */
void do_cross_with_assoc(int level, unsigned int generator) {
    line_num i;

    stat[level + 1].prev_polygon_cnt = d[generator + 1];
    polygon_stat[d[generator + 1]]++;

    d[generator + 1] = 0;
    d[generator + 3]++;

    i = a[generator];
    if (generator > 0) {
        d[generator - 1]++;

        a[generator] = a[generator - 1];
        a[generator - 1] = a[generator + 1];
    }
    else {
        a[generator] = a[generator + 1];
    }
    a[generator + 1] = a[generator + 2];
    a[generator + 2] = i;
}

/**
 * Отмена применения белого генератора и двух ассоциированных (соседних) черных.
 */
void do_uncross_with_assoc(int level, unsigned int generator) {
    line_num i;

    i = a[generator + 2];
    a[generator + 2] = a[generator + 1];

    if (generator > 0) {
        d[generator - 1]--;

        a[generator + 1] = a[generator - 1];
        a[generator - 1] = a[generator];
    }
    else {
        a[generator + 1] = a[generator];
    }
    a[generator] = i;

    d[generator + 3]--;
    d[generator + 1] = stat[level + 1].prev_polygon_cnt;
    polygon_stat[d[generator + 1]]--;

}

/**
 * Метод обеспечивает перебор генераторов. Если на вход установленые ненулевые уровни,
 * метод ожидает установленных полей stat[].rearr_index и stat[].rearrangement
 *
 * @param   int level     Уровень, начиная с которого идет перебор
 * @param   int min_level Уровень, до которого нужно подняться, не дальше от к корня, чем level
 *
 * @return  void
 */
void run (int level, int min_level, unsigned long int iterations) {
	int curr_generator, i;
	Message message;

	for (int i = n + 1; i--; ) {
		d[i] = i == 1 ? 0 : 1;
	}

    // Оптимизация 0. Первые генераторы должны образовать (n-1)/2 внешних черных двуугольников.
    for (int i = n / 2; i--; ) {
        set(i * 2 + 1, 1);
    }

	// Initializing
	for (i = 0; i <= n_step; ++i) {
		stat[i].processed = 0;
	}

    // Восстанавливаем генераторы на основе rearr_index, установленных перед запуском
	for (i = 0; i < level; ++i) {
		stat[i + 1].generator = curr_generator = stat[i].generator + triplets[stat[i].rearrangement][stat[i].rearr_index];
		stat[i + 1].processed++;
		stat[i + 1].prev_polygon_cnt = d[curr_generator + 1];
//		d[curr_generator + 1] = 0;
//		d[curr_generator]++;
//		d[curr_generator + 2]++;
		do_cross_with_assoc(i, curr_generator);
	}

	was_alarm = 0;

	// Run
	while (1) {
        iterations++; // Раскомментировать при добавлении новых условий оптимизации для "профилировки".
		// printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> it=%d  lev=%d ri=%d maxi=%d, cond=%d\n", iterations, level, stat[level].rearr_index, (n - 3)/2, stat[level].rearr_index < (int)(n - 3)/2);
		if (stat[level].rearr_index < (int) (n - 3)/2) {
			stat[level].rearr_index++;
			curr_generator = stat[level].generator + triplets[stat[level].rearrangement][stat[level].rearr_index];
			// printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> next, curr_generator = %d, %d\n", curr_generator, triplets[stat[level].rearrangement][stat[level].rearr_index]);


			if (curr_generator > n-3 || curr_generator < 0)
				continue;
			if (a[curr_generator] > a[curr_generator + 1])
				continue;



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
             * Остальные генераторы применять в этом интервале не нужно. Для отслеживания начала движения
             * последней прямой используется a[n-2], а не a[n-1] в силу оптимизации 0.
             */

            // if (polygon_stat[12-4] > 0) {
            //     continue;
            // }
            // if (polygon_stat[11-4] > 0) {
            //     continue;
            // }
            // if (polygon_stat[10-4] > 0) {
            //     continue;
            // }
            // if (polygon_stat[9-4] > 1) {
            //     continue;
            // }
            // if (polygon_stat[8-4] > 2) {
            //     continue;
            // }
            // if (polygon_stat[7-4] > 17) {
            //     continue;
            // }


            if (curr_generator != 0) {
                // Оптимизация 1 и 3.
                // На самом деле если ее убрать, в перебор пойдут недопустимые наборы генераторов
                if (a[curr_generator - 1] > a[curr_generator + 1]) {
                    continue;
                }

                // Оптимизация 1 и 3.
                // На самом деле если ее убрать, в перебор пойдут недопустимые наборы генераторов
                if (a[curr_generator] > a[curr_generator + 2]) {
                    continue;
                }

                // Оптимизация 2.
                if (a[curr_generator] + 1 == a[curr_generator + 2] && curr_generator + 1 + a[curr_generator + 2] != n - 1) {
                    continue;
                }

                // Оптимизация 1.
                // Как показывает профилировка, после остальных оптимизаций эта не дает ускорения
                // if (a[curr_generator] > a[curr_generator + 1]) {
                //     continue;
                // }

                // Оптимизация 2.
                if (a[curr_generator - 1] + 1 == a[curr_generator + 1] && curr_generator - 1 + a[curr_generator + 1] != n - 1) {
                    continue;
                }

                // Оптимизация 2. Закомментировано, потому что профилировка показывает, что количество итераций слегка сокращается, а время выполнения увеличивается.
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
            // if (a[0] != n-1 && a[n-2] != n-1 && a[curr_generator + 1] != n-1) {
            //     continue;
            // }

            // if (d[curr_generator + 1] + 4 > 8) {
            //     continue;
            // }

            // Ограничение на поиск: максимальный генератор можно применить только два раза.
            // Слишком сильное, долгий перебор, но может пригодится в каких-нибудь эвристиках.
            // if (curr_generator == n - 3 && a[curr_generator + 1] != n-1 && a[curr_generator] != 0) {
            //     continue;
            // }


			stat[level + 1].generator = curr_generator;
			stat[level + 1].processed++;

			if (n_step -1 == level) {
				// printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> count_gen!!! >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
				count_gen(level, iterations);
				continue;
			}

			// stat[level + 1].prev_polygon_cnt = d[curr_generator + 1];
			// d[curr_generator + 1] = 0;
			// d[curr_generator]++;
			// d[curr_generator + 2]++;
            // Применяем белый генератор curr_generator и ассоциированные соседние черные генераторы
			// printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> down\n");
			do_cross_with_assoc(level, curr_generator);
			level++;
			stat[level].rearrangement = 0;
			stat[level].rearr_index = -1;

			if (was_alarm) {
				trace("%s Alarm\n", NODE_NAME);

				was_alarm = 0;
				alarm(TQ);
				for (i = min_level; i < n_step; ++i) {
					if (stat[i + 1].processed > 1)
						break;
				}

				message.min_level = min_level; 
				message.level = i; 
				min_level = i; 
				min_level = message.level = i; // remove?
				message.w_level = level;
				message.iterations = iterations;
				message.status = FORKED;

				trace("%s message.level = %d\n", NODE_NAME, message.level);
				for (i = 0; i <= message.w_level; ++i) {
					message.rearr_index[i] = stat[i].rearr_index;
					message.rearrangement[i] = stat[i].rearrangement;
				}
				message.max_s = max_s;

				send_message_to_dispatcher(&message);

				for (i = 0; i <= n_step; ++i)
					stat[i].processed = 1;
			}
		}
		else {
			// printf(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> up\n");
			
			curr_generator = stat[level].generator;
            stat[level].iterations = 0; // ???
			level--;
			do_uncross_with_assoc(level, curr_generator);
			// d[curr_generator]--;
			// d[curr_generator + 2]--;
			// d[curr_generator + 1] = stat[level + 1].prev_polygon_cnt;
			if (level == min_level) {
				break;
			}
		}
	}
}

typedef struct { 
	int rearrangement[n_step1]; 
	int rearr_index[n_step1]; 
	int min_level, level;
	unsigned long int iterations; 
	int w_idle_time, w_run_time, network_time; 
	struct timeval t; 
} worker_info; 

worker_info *workers_info; 

void print_timings() { 
	FILE *f; 
	char filename[64]; 
	sprintf(filename, "%d_timings.txt", n); 
	f = fopen(filename, "a"); 
	fprintf(f, 
		"w_run_time = %dms\n" 
		"w_idle_time = %dms\n" 
		"d_push_msg_time = %du, count = %d, avg = %du\n" 
		"d_run_time = %dms\n" 
		"d_idle_time = %dms\n" 
		"network_time = %dms\n", 
		timings.w_run_time, 
		timings.w_idle_time, 
		timings.d_push_msg_time, timings.push_count, timings.d_push_msg_time/timings.push_count, 
		timings.d_run_time, 
		timings.d_idle_time, 
		timings.network_time); 
	fclose(f); 
} 

void copy_timings_from_message(worker_info *inf, const Message *msg) { 
	inf->w_idle_time  = msg->w_idle_time; 
	inf->w_run_time   = msg->w_run_time; 
	inf->network_time = msg->network_time; 
	gettimeofday(&inf->t, NULL); 
} 

void copy_gens_from_message(worker_info *inf, const Message *msg) { 
	memcpy(inf->rearrangement, msg->rearrangement, sizeof(msg->rearrangement)); 
	memcpy(inf->rearr_index, msg->rearr_index, sizeof(msg->rearr_index)); 
} 

void dump_queue() { 
	static int v = 1, i, j; 
	char filename[256]; 
	FILE *f; 
	v = !v; 

	sprintf(filename, "%d_dump_gens_%d", n, v);
	f = fopen(filename, "w+"); 
	fprintf(f, "// n\n%d\n", n);
	fprintf(f, "// max_s\n");
	fprintf(f, "%d\n", max_s);

	fprintf(f, "// min_lev, lev, iterations, rearrangement, rearr_index ...\n"); 
	for (i = 0; i < wrk_count; ++i) { 
		if (wrk_state[i] == BUSY) { 
			fprintf(f, "%d %d %lu\n", workers_info[i].min_level, workers_info[i].level, workers_info[i].iterations); 
			for (j = 0; j <= workers_info[i].level; ++j) 
				fprintf(f, "%d ", workers_info[i].rearrangement[j]); 
			fprintf(f, "\n"); 
			for (j = 0; j <= workers_info[i].level; ++j) 
				fprintf(f, "%d ", workers_info[i].rearr_index[j]); 
			fprintf(f, "\n"); 
		} 
	} 
	for (i = 0; i < msg_count; ++i) { 
		fprintf(f, "%d %d %lu\n", messages[i]->min_level, messages[i]->level, messages[i]->iterations); 
		for (j = 0; j <= messages[i]->level; ++j) 
			fprintf(f, "%d ", messages[i]->rearrangement[j]); 
		fprintf(f, "\n"); 
		for (j = 0; j <= messages[i]->level; ++j) 
			fprintf(f, "%d ", messages[i]->rearr_index[j]); 
		fprintf(f, "\n"); 
	} 

	fclose(f); 
} 

void load_queue(const char *filename, int is_dispatcher) { 
	FILE *f; 
	int l, i, res; 
	Message *msg; 
	char *str = (char *) malloc(65536), *ptr; 
	l = 0; 

	f = fopen(filename, "r"); 
	if (!f) { 
		printf("File %s not found, exit\n", filename); 
		exit(0); 
	} 
	while(res = fscanf(f, "%[^\n]\n", str), res != -1 && res != 0) { 
		if (str[0] == '/') {
			continue; 
		}
		switch (l) { 
			case 0: 
				ptr = str;
				n = strtol(ptr, &ptr, 10); 
				if (!is_dispatcher) {
					goto load_queue_exit;
				}
				l = 2; 
				break; 
			case 1:
			 	ptr = str;
		    	max_s = strtol(ptr, &ptr, 10);
			 	l = 2;
			 	break;
			case 2: 
				msg = (Message *) malloc(sizeof(Message)); 
				sscanf(str, "%d %d %lu", &msg->min_level, &msg->level, &msg->iterations); 
				l = 3; 
				break; 
			case 3: 
				ptr = str; 
				for (i = 0; i <= msg->level; i++ ) { 
					msg->rearrangement[i] = strtol(ptr, &ptr, 10); 
				} 
				l = 4; 
				break; 
			case 4: 
				ptr = str; 
				for (i = 0; i <= msg->level; i++ ) { 
					msg->rearr_index[i] = strtol(ptr, &ptr, 10); 
				} 
			    msg->max_s = max_s;
				msg->status = BUSY; 
				push_msg_back(msg); 
				l = 2; 
				break; 
		} 
	} 
load_queue_exit:
	fclose(f); 
	free(str); 
} 

void send_work(int node, Message *msg) { 
	MPI_Send((void *)msg, sizeof(Message)/sizeof(int), MPI_INT, node, TAG, MPI_COMM_WORLD); 

	copy_gens_from_message(&workers_info[node-1], msg); 
	workers_info[node-1].level = msg->level; 
	workers_info[node-1].min_level = msg->min_level;
	workers_info[node-1].iterations = msg->iterations;
}

void do_dispatcher(int numprocs, const char *dump_filename) {
	Message message, *msg;
	MPI_Status status;
	int worker, i;

	struct timeval t1, t2, t3, t4; 
	int push_time, idle_time, run_time; 
	int timeings_results; 

	timings.d_idle_time = timings.d_push_msg_time = timings.d_run_time = 0; 
	signal(SIGALRM, tmr); 
	signal(SIGUSR1, send_timings_signal); 

	printf("%s We have %d processes\n", NODE_NAME, numprocs);

	init_messages_stack();
	init_worker_state(numprocs - 1);

	workers_info = (worker_info *) malloc((wrk_count)*sizeof(worker_info)); 

	for (i = 0; i < wrk_count; ++i) { 
		memset((void *) &workers_info[i], 0, sizeof(worker_info)); 
		gettimeofday(&workers_info[i].t, NULL); 
	} 

	message.max_s = max_s = 0;

	if (!dump_filename[0]) { 
		// Sending first peace of work (root) to the first worker 
		message.level = 0; 
		message.min_level = 0; 
		message.iterations = 0; 
		message.status = BUSY; 
		message.rearrangement[0] = 0; 
		workers_info[0].rearr_index[0] = message.rearr_index[0] = -1; 
		set_wrk_state(1, BUSY); 
		MPI_Send((void *)&message, sizeof(Message)/sizeof(int), MPI_INT, 1, TAG, MPI_COMM_WORLD); 
	} else { 
		load_queue(dump_filename, 1); 
		while ((worker = get_worker(FINISHED)) != -1) { 
			if (msg = pop_msg()) { 
				trace("%s Sending a peace of work to the node %d, pop from stack\n", NODE_NAME, worker); 
				send_work(worker, msg); 
				free(msg); 
				set_wrk_state(worker, BUSY); 
			} else {
				break; 
			} 
		} 
	} 

	alarm(TDUMP); 

	// Main loop
	gettimeofday(&t3, NULL);
	for (;;) {
		if (get_timings) { 
			get_timings = 0; 
			timings.w_run_time = timings.w_idle_time = timings.network_time = 0; 

			for (i = 0; i < wrk_count; ++i) { 
				timings.w_run_time += workers_info[i].w_run_time; 
				timings.w_idle_time += workers_info[i].w_idle_time; 
				timings.network_time += workers_info[i].network_time; 
			} 

			print_timings(); 
			dump_queue(); 
		} 

		if (was_alarm) { 
			was_alarm = 0; 
			alarm(TDUMP); 
			dump_queue(); 
		} 

		trace("%s Waiting for a message\n", NODE_NAME);
		memset((void *)&message, 0, sizeof(Message));
		gettimeofday(&t4, NULL); 
		run_time =  (t4.tv_sec - t3.tv_sec)*1000 + (t4.tv_usec - t3.tv_usec)/1000; 
		timings.d_run_time += run_time; 
		MPI_Recv((void *)&message, sizeof(Message)/sizeof(int), MPI_INT, MPI_ANY_SOURCE, TAG, MPI_COMM_WORLD, &status);
		gettimeofday(&t3, NULL); 

		idle_time =  (t3.tv_sec - t4.tv_sec)*1000 + (t3.tv_usec - t4.tv_usec)/1000; 
		timings.d_idle_time += idle_time;
		message.max_s = max_s = max(message.max_s, max_s);
		copy_timings_from_message(&workers_info[status.MPI_SOURCE-1], &message); 
		switch (message.status) {
			case FORKED:
				trace("%s Have a message, level = %d\n", NODE_NAME, message.level); 

				copy_gens_from_message(&workers_info[status.MPI_SOURCE-1], &message); 
				workers_info[status.MPI_SOURCE-1].level = message.w_level; 
				workers_info[status.MPI_SOURCE-1].min_level = message.level;
				workers_info[status.MPI_SOURCE-1].iterations = message.iterations;
				worker = get_worker(FINISHED);
				if (worker != -1) {
					set_wrk_state(worker, BUSY);
					message.status = BUSY;
					trace("%s Sending a peace of work to the node %d\n", NODE_NAME, worker); 
					gettimeofday(&t1, NULL); 
					message.d_search_time = (t1.tv_sec - workers_info[worker-1].t.tv_sec)*1000 + (t1.tv_usec - workers_info[worker-1].t.tv_usec)/1000; 
					send_work(worker, &message); 
				}
				else {
					trace("%s Push to stack\n", NODE_NAME); 
					gettimeofday(&t1, NULL);
					msg = (Message *) malloc(sizeof(Message));
					if (msg == NULL) {
						printf ("Panic! Not enough memory!\n");
					}
					memcpy((void *) msg, (void *) &message, sizeof(Message));
					push_msg_back(msg);
					gettimeofday(&t2, NULL); 
					push_time =  (t2.tv_sec - t1.tv_sec)*1000000 + (t2.tv_usec - t1.tv_usec); 
					timings.d_push_msg_time += push_time;
				}
				break;
			case FINISHED:
				trace("%s Have 'finished' message from %d\n", NODE_NAME, status.MPI_SOURCE);
				if (msg = pop_msg()) {
					trace("%s Sending a peace of work to the node %d, pop from stack\n", NODE_NAME, status.MPI_SOURCE); 
					gettimeofday(&t1, NULL); 
					message.d_search_time = (t1.tv_sec - t3.tv_sec)*1000 + (t1.tv_usec - t3.tv_usec)/1000; 
					send_work(status.MPI_SOURCE, msg);
					free(msg);
				}
				else {
					set_wrk_state(status.MPI_SOURCE, FINISHED);
					worker = get_worker(BUSY);
					if (worker == -1) {
						// All workers finished, kill them
						message.status = QUIT;
						for (worker = 1; worker < numprocs; ++worker)
							MPI_Send((void *)&message, sizeof(Message)/sizeof(int), MPI_INT, worker, TAG, MPI_COMM_WORLD);
						return;
					}
				}
				break;
			default:
				printf("%s Status error\n", NODE_NAME);
		}
	}
}

void do_worker(int id, const char *dump_filename) {
	Message message;
	MPI_Status status;
	int i;

	struct timeval t1, t2; 
	int run_time, waiting_for_msg_time; 

	timings.w_run_time = 0; 
	timings.w_idle_time = 0; 
	timings.network_time = 0;

	// Timer initialization
	signal(SIGALRM, tmr);
	signal(SIGUSR1, send_timings_signal);

	for (i = 1; i <= (n-3)/2; i++ ) {
		triplets[0][i] = n-1 - 2*i;
	}

	triplets[0][0] = -2;

	stat[0].generator = 0;

	if (dump_filename[0]) { 
		load_queue(dump_filename, 0); 
	} 

	gettimeofday(&t2, NULL);

	for(;;) {
		// receive from dispatcher:
		trace("%s Waiting for a message from the dispatcher\n", NODE_NAME);
		MPI_Recv((void *)&message, sizeof(Message)/sizeof(int), MPI_INT, 0, TAG, MPI_COMM_WORLD, &status);
		gettimeofday(&t1, NULL);

		waiting_for_msg_time =  (t1.tv_sec - t2.tv_sec)*1000 + (t1.tv_usec - t2.tv_usec)/1000; 
		timings.w_idle_time += waiting_for_msg_time; 
		timings.network_time += (waiting_for_msg_time - message.d_search_time); 
		if (message.status == QUIT) {
			trace("%s Received 'quit' message\n", NODE_NAME);
			break;
		}

		trace("%s Received a message from the dispatcher\n", NODE_NAME);

		max_s = message.max_s;

		for (i = 0; i <= message.level; ++i) {
			stat[i].rearrangement = message.rearrangement[i];
			stat[i].rearr_index = message.rearr_index[i];
		}

		b_free = (n*(n-1) / 2) - 1; // почему -1?
		n_step = (n * (n - 2) + 3) / 6;
		for (i = 0; i < n; i++) {
			a[i] = i;
		}

		trace("%s Run, level = %d, minlevel = %d, iterations = %lu\n", NODE_NAME, message.level, message.min_level, message.iterations);

		alarm(TQ);
		run(message.level, message.min_level, message.iterations);
		gettimeofday(&t2, NULL); 
		run_time =  (t2.tv_sec - t1.tv_sec)*1000 + (t2.tv_usec - t1.tv_usec)/1000; 
		timings.w_run_time += run_time; 
		message.status = FINISHED;
		trace("%s Finished\n", NODE_NAME); 

		send_message_to_dispatcher(&message);
	}
}

int main(int argc, char **argv) {
	int i;
	char s[80], dump_filename[80];
	int numprocs;
	int myid;
	MPI_Status mpi_stat; 

	if (argc < 2) {
		printf(
			"Usage: %s -n N [-o filename] [-full] [-d dump_filename]\n"
			"-n             line count;\n"
			"-full          output all found generator sets;\n"
			"-d             dump file name\n"
			"-o             output file.\n", argv[0]);
		return 0;
	}

	dump_filename[0] = '\0';

	for (i = 1; i < argc; i++) {
		strcpy(s, argv[i]);
		if (!strcmp("-n", s))
			sscanf(argv[++i], "%d", &n);
		else if (!strcmp("-o", s))
			strcpy(filename, argv[++i]);
		else if (!strcmp("-full", s))
			full = 1;
		else if (!strcmp("-d", s)) 
			strcpy(dump_filename, argv[++i]);
		else {
			printf(
				"Usage: %s -n N [-o filename] [-full] [-d dump_filename]\n"
				"-n             line count;\n"
				"-full          output all found generator sets;\n"
				"-d             dump file name\n"
				"-o             output file.\n", argv[0]);
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

	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD,&numprocs);
	MPI_Comm_rank(MPI_COMM_WORLD,&myid);

    gettimeofday(&start, NULL);

	if (myid == 0) {
		strcpy(NODE_NAME, "Dispatcher:");
		do_dispatcher(numprocs, dump_filename);
	}
	else {
		sprintf(NODE_NAME, "Worker %d:", myid);
		do_worker(myid, dump_filename);
	}
    struct timeval end;

    gettimeofday(&end, NULL);
    double time_taken = end.tv_sec + end.tv_usec / 1e6 -
        start.tv_sec - start.tv_usec / 1e6; // in seconds

	printf("---------->>> [%fs] %s terminated.\n", time_taken, NODE_NAME);
	MPI_Finalize();
	
	return 0;
}