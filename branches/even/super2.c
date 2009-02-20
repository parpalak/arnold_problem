#include <stdio.h>
#include <strings.h>

#define n1 40
#define n_step1 (n1*(n1-1) / 2)

#define plurality 3
#define rearrangement_count 3

int n, n_step, n_def_gen;

int triplets[rearrangement_count][n1]; //= {{1, 2, -1}, {2, -1, 1}, {-1, 2, 1}};

typedef struct {
	int defects, generator, stack;
	int rearrangement;
	int rearr_index;
} Stat;

Stat stat[n_step1 + 1];

int def_gen[n_step1];
int a[n1], d[n1 + 1];

int max_s[n1/2 + 1];

int b_free;
int max_defects;

// User input
char filename[80] = "";
int max_defects_default = 0;
int full = 0;
int show_stat = 0;

void find_parallel_config (int k, int level) {
	int s, i, max_defects1;
	FILE *f;

	for (i = 0; i < n; i++)
		if (a[i] + i < n-2 || a[i] + i > n)
			return;

	s = - 1 + (n & 1);
	for (i = 1; i <= level + 1; i++) {
		s -= (stat[i].generator & 1)*2 - 1;
	}
	if (s < 0)
		s = -s;
	if (s < max_s[k] || !full && s == max_s[k])
		return;

	max_s[k] = s;
	max_defects1 = ((n*(n+1)/2 - k - 3) - 3*max_s[k])/2;
	//max_defects = max_defects > max_defects1 ? max_defects1 : max_defects;

	if (strcmp(filename, "") == 0) {
		printf("A(%d, %d) = %d; %d %d)", n, k, s, max_defects, max_defects1);
		for (i=1; i <= level+1; i++) {
			printf(" %d", stat[i].generator);
		}

		printf("\n");
		if (show_stat)
			for (i = 0; i < n/2 + 1; i++ )
				printf("A(%d, %d) = %d;\n", n, i, max_s[i]);

		fflush(NULL);
		fflush(NULL);
		fflush(stdout);
		fflush(stdout);
	}
	else {
		f = fopen(filename, "a");
		
		fprintf(f, "A(%d, %d) = %d; %d %d)", n, k, s, max_defects, max_defects1);
		for (i=1; i <= level+1; i++) {
			fprintf(f, " %d", stat[i].generator);
		}

		fprintf(f, "\n");
		if (show_stat)
			for (i = 0; i < n/2 + 1; i++ )
				fprintf(f, "A(%d, %d) = %d;\n", n, i, max_s[i]);
		fclose(f);
	}
}

void inline set (int curr_generator, int t) {
	int i;

	b_free += -2*t+1;
	i = a[curr_generator];
	a[curr_generator] = a[curr_generator + 1];
	a[curr_generator + 1] = i;
}

void new1 (int level) {
	int curr_generator, direct, i;

	for (i = n/2; i--; )
		max_s[i] = 0;
	max_defects = max_defects_default;

	stat[level].rearr_index = -1;
	for (i = n + 1; i--; ) {
		d[i] = 2;
	}

	while (1) {
		if (stat[level].rearr_index < plurality - 1) {
			stat[level].rearr_index++;
			curr_generator = stat[level].generator + (direct = triplets[stat[level].rearrangement][stat[level].rearr_index]);
			if (curr_generator > n-2 || curr_generator < 0)
				continue;
			if (a[curr_generator] > a[curr_generator + 1])
				continue;
			stat[level + 1].generator = curr_generator;
			if (b_free <= n/2) {
//				printf("%d\n", b_free);
				set(curr_generator, 1);
				find_parallel_config(b_free + 1, level);
				set(curr_generator, 0);
			}
			if (!b_free) {
//				count_gen(level);
				continue;
			}
			stat[level + 1].defects = stat[level].defects;
			if (!(curr_generator % 2)/* && (b_free*2 > n)*/) { // even, white
				if (d[curr_generator + 1] < 3) // '<3' for 0 defects, '==1' for full search
					continue;
				if (d[curr_generator] == 1) {
					stat[level + 1].defects++;
				}
				if (d[curr_generator + 2] == 1) {
					stat[level + 1].defects++;
				}
				if (stat[level + 1].defects > max_defects) {
					continue;
				}
			}
			stat[level+1].stack = d[curr_generator + 1];
			d[curr_generator + 1] = 0;
			d[curr_generator]++;
			d[curr_generator + 2]++;
			set(curr_generator, 1);
			level++;
//			stat[level].rearrangement = direct > 0 ? 1 : 2;
			if (direct > 0) {
				if (curr_generator % 2)
					stat[level].rearrangement = 1;
				else
					stat[level].rearrangement = 0;
			}
			else
			    stat[level].rearrangement = 2;
			stat[level].rearr_index = -1;
		}
		else {
			curr_generator = stat[level].generator;
			level--;
			set(curr_generator, 0);
			d[curr_generator]--;
			d[curr_generator + 2]--;
			d[curr_generator + 1] = stat[level+1].stack;
			if (!level)
				break;
		}
	}
}

int main(int argc, char **argv) {
	int i;
	char s[80];

	if (argc < 2) {
		printf("Usage: %s -n N [-max-def D] [-o filename] [-full] [-stat]\n  -n\n     line count;\n  -max-def\n     the number of allowed defects;\n  -full\n     output all found generator sets;\n  -stat\n     show stat after every generator set;\n  -o\n     output file.\n", argv[0]);
		return 0;
	}

	for (i = 1; i < argc; i++) {
		strcpy(s, argv[i]);
		if (0 == strcmp("-n", s))
			sscanf(argv[++i], "%d", &n);
		else if (0 == strcmp("-max-def", s))
			sscanf(argv[++i], "%d", &max_defects_default);
		else if (0 == strcmp("-o", s))
			strcpy(filename, argv[++i]);
		else if (0 == strcmp("-full", s))
			full = 1;
		else if (0 == strcmp("-stat", s))
			show_stat = 1;
		else {
			printf("Invalid parameter: \"%s\".\nUsage: %s -n N [-max-def D] [-o filename] [-full] [-stat]\n  -n\n     line count;\n  -max-def\n     the number of allowed defects;\n  -full\n     output all found generator sets;\n  -stat\n     show stat after every generator set;\n  -o\n     output file.\n", s, argv[0]);
			return 0;
		}
	}

	b_free = (max_defects = n_step = n*(n-1) / 2) - 1;

/*	n_def_gen = argc - 2;
	for (i = 0; i < n_def_gen; i++)
		sscanf(argv[i + 2], "%d", def_gen + i);
*/
	for (i = 0; i < n; i++) {
		a[i] = i;
	}
	
	//{{1, 2, -1}, {2, -1, 1}, {-1, 2, 1}};

	for (i = 3; i < n1; i++ ) {
		triplets[0][i] = triplets[1][i] = triplets[2][i] = i;
	}

	triplets[0][0] = 1;
	triplets[0][1] = 2;
	triplets[0][2] = -1;
	triplets[1][0] = 2;
	triplets[1][1] = -1;
	triplets[1][2] = 1;
	triplets[2][0] = -1;
	triplets[2][1] = 2;
	triplets[2][2] = 1;

	stat[0].rearrangement = 0;
	stat[0].generator = 0;
	stat[0].defects = 0;

	new1(0);

	printf("---------->>>Process terminated.\n");
	for (i = n/2 + 1; i--; )
		printf("A(%d, %d) = %d;\n", n, i, max_s[i]);
	return 0;
}