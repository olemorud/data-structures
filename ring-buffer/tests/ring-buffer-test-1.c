// This test is generated entirely by ChatGPT
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sched.h>

#define RB_T    int
#define RB_SIZE 128
#include "ring-buffer.h"

enum { TOTAL = 127 };

static struct rb_int_128 rb;

static int values[TOTAL];
static _Atomic int prod_idx;
static _Atomic int cons_count;
static _Atomic long long sum_enq;
static _Atomic long long sum_deq;

static void *producer(void *arg)
{
    (void)arg;
    for (;;) {
        int i = atomic_fetch_add_explicit(&prod_idx, 1, memory_order_relaxed);
        if (i >= TOTAL) break;

        int v = values[i];
        atomic_fetch_add_explicit(&sum_enq, (long long)v, memory_order_relaxed);

        while (!rb_int_128_enqueue(&rb, v)) {
            sched_yield();
        }
    }
    return NULL;
}

static void *consumer(void *arg)
{
    (void)arg;
    for (;;) {
        int done = atomic_load_explicit(&cons_count, memory_order_relaxed);
        if (done >= TOTAL) break;

        int v;
        if (rb_int_128_dequeue(&rb, &v)) {
            atomic_fetch_add_explicit(&sum_deq, (long long)v, memory_order_relaxed);
            atomic_fetch_add_explicit(&cons_count, 1, memory_order_relaxed);
        } else {
            sched_yield();
        }
    }
    return NULL;
}

int main(void)
{
    rb_int_128_init(&rb);

    int sum = 0;
    for (int i = 0; i < TOTAL; i++) {
        int n = (i * 31 + 7) % 1000 - 500;
        values[i] = n;
        sum += n;
    }
    fprintf(stderr, "expecting %d\n", sum);

    const int P = 4;
    const int C = 4;
    pthread_t pt[P], ct[C];

    for (int i = 0; i < P; i++) {
        if (pthread_create(&pt[i], NULL, producer, NULL) != 0) return EXIT_FAILURE;
    }
    for (int i = 0; i < C; i++) {
        if (pthread_create(&ct[i], NULL, consumer, NULL) != 0) return EXIT_FAILURE;
    }

    for (int i = 0; i < P; i++) {
        if (pthread_join(pt[i], NULL) != 0) return EXIT_FAILURE;
    }
    for (int i = 0; i < C; i++) {
        if (pthread_join(ct[i], NULL) != 0) return EXIT_FAILURE;
    }

    long long a = atomic_load_explicit(&sum_enq, memory_order_relaxed);
    long long b = atomic_load_explicit(&sum_deq, memory_order_relaxed);
    int n = atomic_load_explicit(&cons_count, memory_order_relaxed);

    if (n != TOTAL) {
        fprintf(stderr, "FAIL: consumed %d/%d\n", n, TOTAL);
        return EXIT_FAILURE;
    }
    if (a != b) {
        fprintf(stderr, "FAIL: sum_enq=%lld sum_deq=%lld\n", a, b);
        return EXIT_FAILURE;
    }

    printf("PASS: consumed=%d sum=%lld\n", n, a);
    return EXIT_SUCCESS;
}

