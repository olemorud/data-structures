
#include "hashmap.c"

#define HASHMAP_VAL int64_t
#define HASHMAP_PREFIX hashmap_int64
#include "hashmap.c"

#define HASHMAP_VAL double
#define HASHMAP_PREFIX hashmap_double
#include "hashmap.c"

#pragma GCC diagnostic ignored "-Wunused-variable"

#include <dirent.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <errno.h>

typedef size_t (*hash_func)(const void*, size_t);

static struct timespec timediff(struct timespec start, struct timespec end)
{
    struct timespec temp;
    if ((end.tv_nsec - start.tv_nsec)<0) {
        temp.tv_sec = end.tv_sec-start.tv_sec-1;
        temp.tv_nsec = 1000000000+end.tv_nsec-start.tv_nsec;
    } else {
        temp.tv_sec = end.tv_sec-start.tv_sec;
        temp.tv_nsec = end.tv_nsec-start.tv_nsec;
    }
    return temp;
}

static double mean(double nums[], size_t len)
{
    double sum = 0;
    for (size_t i = 0; i < len; i++) {
        sum += nums[i];
    }
    return sum / (double)len;
}

static double variance(double nums[], size_t len)
{
    if (len <= 1) {
        return 0;
    }
    double sum = 0;
    double m = mean(nums, len);
    for (size_t i = 0; i < len; i++) {
        /* (x_i - x_hat)^2 */
        double n = (nums[i] - m);
        n *= n;
        sum += n;
    }
    return sum / (len-1);
}

static double stddev(double nums[], size_t len)
{
    return sqrt(variance(nums, len));
}

/* b - buckets
 * m - number of buckets
 * n - number of items */
double chisquare(double b[], size_t m, int n)
{
    double res = 0;
    for (size_t i = 0; i < m; i++) {
        res += b[i] * b[i];
    }
    const double nd = (double)n;
    const double md = (double)m;
    res = res * md/nd - nd;
    return res;
}

static double hash_throughput(hash_func f)
{
    struct timespec start, end, elapsed;
    const uint64_t is = 1<<21;

    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &start);

    for (uint64_t i = 0; i < is; i++) {
        const uint64_t counter[2] = {i / 8, i % 8};
        volatile uint64_t h = f(counter, sizeof counter);
        (void)h;
    }
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &end);
    elapsed = timediff(start, end);
	double d_elapsed = (double)elapsed.tv_sec + (double)elapsed.tv_nsec/1e9;

    double throughput = ((double)is)/d_elapsed;
    return throughput;
}

struct distribution {
    double stddev;
    double mean;
    double chisquare;
    double expected_chisquare;
};

static struct distribution hash_distribution_wordlist(hash_func hash)
{
    FILE* f = fopen("wordlist.txt", "r");
    if (f == NULL) {
        fprintf(stderr, "fatal: failed to open wordlist.txt: %m\n");
        abort();
    }

    double buckets[ENTRY_COUNT] = {0};
    char word[1024];
    int words = 0;
    while (fscanf(f, " %1023s\n", word) == 1) {
        volatile size_t h = hash(word, strlen(word));
        buckets[h % (sizeof buckets / sizeof *buckets)]++;
        words++;
    }

    double x2 = chisquare(buckets, sizeof buckets / sizeof *buckets, words);

    return (struct distribution){
        .stddev = stddev(buckets, sizeof buckets / sizeof *buckets),
        .mean = mean(buckets, sizeof buckets / sizeof *buckets),
        .chisquare = x2,
        .expected_chisquare = sizeof buckets / sizeof *buckets - 1,
    };
}

static void walk(const char *base, int depth, int* count, double* buckets, size_t buckets_size, hash_func hash)
{
    constexpr typeof(depth) MAX_DEPTH = 64;
    constexpr size_t MAX_PATH = 4096;
    constexpr int MAX_SAMPLES = 1<<21;
    if (depth >= MAX_DEPTH)
        return;

    DIR *d = opendir(base);
    if (!d)
        return;

    char path[MAX_PATH];
    struct dirent *de;

    while ((de = readdir(d))) {
        /* skip '.' and '..' */
        if ((de->d_name[0] == '.' && de->d_name[1] == '\0')
         || (de->d_name[0] == '.' && de->d_name[1] == '.' && de->d_name[2] == '\0'))
            continue;

        /* truncated – skip */
        int n = snprintf(path, sizeof(path), "%s/%s", base, de->d_name);
        if (n + 1 >= (int)sizeof(path))
            continue;
        
        /* handle the entry */
        size_t h = hash(path, n);
        buckets[h % buckets_size] += 1;
        *count += 1;
        if (*count > MAX_SAMPLES)
            return;

        struct stat st;
        if (lstat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
            walk(path, depth + 1, count, buckets, buckets_size, hash);
            if (*count > MAX_SAMPLES)
                return;
        }
    }
    closedir(d);
}

static struct distribution hash_distribution_filesystem(hash_func hash)
{
    double buckets[ENTRY_COUNT] = {0};
    int n = 0;

    walk("/", 0, &n, buckets, sizeof buckets / sizeof *buckets, hash);

    double x2 = chisquare(buckets, sizeof buckets / sizeof *buckets, n);

    return (struct distribution){
        .stddev             = stddev(buckets, sizeof buckets / sizeof *buckets),
        .mean               = mean(buckets, sizeof buckets / sizeof *buckets),
        .chisquare          = x2,
        .expected_chisquare = sizeof buckets / sizeof *buckets - 1,
    };
}

static struct distribution hash_distribution_uneven_rand(hash_func hash)
{
    double buckets[ENTRY_COUNT] = {0};
    const size_t samples = 1<<21;

    for (size_t _ = 0; _ < samples; _++) {
        uint32_t data[8];
        for (size_t i = 0; i < sizeof data / sizeof *data; i++) {
            data[i] = (rand() % (sizeof buckets / sizeof *buckets)) / 2;
        }
        const size_t h = hash(data, sizeof data);
        buckets[h % (sizeof buckets / sizeof *buckets)] += 1;
    }

    double x2 = chisquare(buckets, sizeof buckets / sizeof *buckets, samples);

    return (struct distribution){
        .stddev             = stddev(buckets, sizeof buckets / sizeof *buckets),
        .mean               = mean(buckets, sizeof buckets / sizeof *buckets),
        .chisquare          = x2,
        .expected_chisquare = sizeof buckets / sizeof *buckets - 1,
    };
}

int main()
{
	int status = EXIT_SUCCESS;

    const hash_func hashes[] = { hash_djb2,   jenkins_one_at_a_time_hash,};
    const char* names[]      = {"hash_djb2", "jenkins_one_at_a_time_hash"};

    /* Test hashmap */
    struct arena a = arena_new();
	{
		Hashmap* hm = hashmap_new(&a);

		{ /* test sinsert and sget */
			char key[] = "hello";
			char val[] = "world";
			*hashmap_sinsert(hm, key) = val;
			bool ok = memcmp(hashmap_sget(hm, key, "(not found)"), val, sizeof val) == 0;
			status = ok && status;
			printf("(hashmap) sinsert and sget - %s\n", ok ? "OK" : "FAILED");
		}

		{ /* test insert and get */
			uint8_t key[] = {1,2,3};
			char val[] = "bing bong";
			*hashmap_insert(hm, key, sizeof key) = val;
			bool ok = memcmp(hashmap_get(hm, key, sizeof key, NULL), val, sizeof val) == 0;
			status = ok && status;
			printf("(hashmap) insert and get - %s\n", ok ? "OK" : "FAILED");
		}
		arena_reset(&a);
	}

	{
		Hashmap_int64_t* hm = hashmap_int64_new(&a);
		{ /* test sinsert and sget */
			char key[] = "hello";
			int64_t val = 123;
			*hashmap_int64_sinsert(hm, key) = val;
			bool ok = hashmap_int64_sget(hm, key, -1) == val;
			status = ok && status;
			printf("(hashmap_int64) sinsert and sget - %s\n", ok ? "OK" : "FAILED");
		}

		{ /* test insert and get */
			uint8_t key[] = {1,2,3};
			int64_t val = 321;
			*hashmap_int64_insert(hm, key, sizeof key) = val;
			bool ok = hashmap_int64_get(hm, key, sizeof key, -1) == val;
			status = ok && status;
			printf("(hashmap_int64) insert and get - %s\n", ok ? "OK" : "FAILED");
		}
		arena_reset(&a);
	}

	{
		Hashmap_double* hm = hashmap_double_new(&a);
		{ /* test sinsert and sget */
			char key[] = "hello";
			double val = 123.456;
			*hashmap_double_sinsert(hm, key) = val;
			bool ok = hashmap_double_sget(hm, key, NAN) == val;
			status = ok && status;
			printf("(hashmap_double) sinsert and sget - %s\n", ok ? "OK" : "FAILED");
		}

		{ /* test insert and get */
			uint8_t key[] = {1,2,3};
			double val = 654.321;
			*hashmap_double_insert(hm, key, sizeof key) = val;
			bool ok = hashmap_double_get(hm, key, sizeof key, NAN) == val;
			status = ok && status;
			printf("(hashmap_double) insert and get - %s\n", ok ? "OK" : "FAILED");
		}
		arena_reset(&a);
	}

#if 1
    /* benchmark hash functions */
    for (size_t i = 0; i < sizeof hashes / sizeof *hashes; i++) {
        struct distribution dist_wordlist = hash_distribution_wordlist(hashes[i]);
        struct distribution dist_filesystem = hash_distribution_filesystem(hashes[i]);
        struct distribution dist_rand = hash_distribution_uneven_rand(hashes[i]);
        double throughput = hash_throughput(hashes[i]);
        printf("%s:\n"
               "  throughput: %.2e\n"
               "  wordlist:\n"
               "    mean:               %.02lf\n"
               "    stddev:             %.02lf (%.02lf%%)\n"
               "    chisquare expected: %.02lf\n"
               "    chisquare:          %.02lf(%+.02lf%%)\n"
               "  file system paths:\n"
               "    mean:               %.02lf\n"
               "    stddev:             %.02lf (%.02lf%%)\n"
               "    chisquare expected: %.02lf\n"
               "    chisquare:          %.02lf (%+.02lf%%)\n"
               "  pseudo-random data:\n"
               "    mean:               %.02lf\n"
               "    stddev:             %.02lf (%.02lf%%)\n"
               "    chisquare expected: %.02lf\n"
               "    chisquare:          %.02lf (%+.02lf%%)\n",
               names[i],

               throughput,

               dist_wordlist.mean,
               dist_wordlist.stddev,
               100.0 * dist_wordlist.stddev / dist_wordlist.mean,
               dist_wordlist.expected_chisquare,
               dist_wordlist.chisquare,
               100 * (1 - dist_wordlist.chisquare / dist_wordlist.expected_chisquare),

               dist_filesystem.mean,
               dist_filesystem.stddev,
               100.0 * dist_filesystem.stddev / dist_filesystem.mean,
               dist_filesystem.expected_chisquare,
               dist_filesystem.chisquare,
               100 * (1 - dist_filesystem.chisquare / dist_filesystem.expected_chisquare),

               dist_rand.mean,
               dist_rand.stddev,
               100.0 * dist_rand.stddev / dist_rand.mean,
               dist_rand.expected_chisquare,
               dist_rand.chisquare,
               100 * (1 - dist_rand.chisquare / dist_rand.expected_chisquare)
           );
    }
#endif

    return status;
}
