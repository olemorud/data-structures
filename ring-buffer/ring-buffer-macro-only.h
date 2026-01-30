/*
 * Macro implementation of Dmitry Vyukov’s Multi-Producer Multi-Consumer
 * queue (DV-MPMC) by Ole Morud.
 *
 * Original reference:
 * - https://web.archive.org/web/20250830190104/https://www.1024cores.net/home/lock-free-algorithms/queues/bounded-mpmc-queue
 * - https://drive.google.com/file/d/1uCefvM3bTnWLFrcYoMxCOKGjWwHJQM2n/view
 *
 * ORIGINAL COPYRIGHT NOTICE & LICENSE
 * ===================================
 *
 *  Multi-producer/multi-consumer bounded queue.
 *  Copyright (c) 2010-2011, Dmitry Vyukov. All rights reserved.
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *     1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *     2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 *  THIS SOFTWARE IS PROVIDED BY DMITRY VYUKOV "AS IS" AND ANY EXPRESS OR
 *  IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 *  OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN
 *  NO EVENT SHALL DMITRY VYUKOV OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 *  INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 *  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 *  OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 *  DAMAGE.
 *
 *  The views and conclusions contained in the software and documentation are
 *  those of the authors and should not be interpreted as representing official
 *  policies, either expressed or implied, of Dmitry Vyukov.
 *
 *  EXAMPLE USAGE
 *  ==================================
 *  
 *  ```
 *  #include <stddef.h>
 *  #include <stdio.h>
 *
 *  // defines `struct rb_int_128`, a ring buffer of size 128 with type int
 *  DEFINE_RING_BUFFER(int, 128);
 *
 *  int main()
 *  {
 *      struct rb_int_128 rb;
 *      RB_INIT(&rb);
 *
 *      bool ok;
 *      rb_enqueue(&rb, 5, &ok);
 *
 *      if (!ok) {
 *          // rb is full
 *          return EXIT_FAILURE;
 *      }
 *
 *      int v;
 *      rb_dequeue(&rb, &v, &ok);
 *      if (!ok) {
 *          // rb is empty
 *          return EXIT_FAILURE;
 *      }
 *
*       // "5"
 *      printf("%d", v);
 *
 *      return EXIT_SUCCESS;
 *  }
 *  ```
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdatomic.h>

#define DEFINE_RING_BUFFER(T, identifier, size) \
typedef struct identifier {                     \
    atomic_size_t head;                         \
    atomic_size_t tail;                         \
    struct {                                    \
        T             value;                    \
        atomic_size_t sequence;                 \
    } items[size];                              \
} identifier

#define rb_len(rb) (sizeof((rb)->items) / sizeof ((rb)->items[0]))

#define rb_init(rb) \
    do { \
        for (size_t i = 0; i < rb_len(rb); ++i) { \
            atomic_store_explicit(&rb->items[i].sequence, i, memory_order_relaxed); \
        } \
        atomic_store_explicit(&rb->head, 0, memory_order_relaxed); \
        atomic_store_explicit(&rb->tail, 0, memory_order_relaxed); \
    } while (0)


/*
    rb_enqueue
    rb     - ring buffer to insert to
    v      - inserted value 
    ok_ptr - underlying value set to true if insertion succeeded,
             false if ring buffer is full
   */
#define rb_enqueue(rb, v, ok_ptr)                                           \
do {                                                                        \
    bool _ok = false;                                                       \
    size_t pos = atomic_load_explicit(&(rb)->tail, memory_order_relaxed);   \
    while (1) {                                                             \
        size_t idx = pos % rb_len(rb);                                      \
        size_t seq = atomic_load_explicit(&(rb)->items[idx].sequence,       \
                                          memory_order_acquire);            \
        intptr_t dif = (intptr_t)seq - (intptr_t)pos;                       \
        if (dif == 0) {                                                     \
            if (atomic_compare_exchange_weak_explicit(                      \
                    &(rb)->tail, &pos, pos + 1,                             \
                    memory_order_relaxed, memory_order_relaxed)) {          \
                (rb)->items[idx].value = (v);                               \
                atomic_store_explicit(&(rb)->items[idx].sequence,           \
                                      pos + 1,                              \
                                      memory_order_release);                \
                _ok = true;                                                 \
                break;                                                      \
            }                                                               \
            /* CAS failed: pos updated, retry */                            \
        } else if (dif < 0) {                                               \
            /* full */                                                      \
            _ok = false;                                                    \
            break;                                                          \
        } else {                                                            \
            /* contention: move to current tail */                          \
            pos = atomic_load_explicit(&(rb)->tail, memory_order_relaxed);  \
        }                                                                   \
    }                                                                       \
    *(ok_ptr) = _ok;                                                        \
} while (0)

/*
    RB_DEQUEUE
    rb      - ring buffer to fetch from
    out_ptr - fetched value 
    ok_ptr  - underlying value set to true if dequeue succeeded,
              false if ring buffer is empty
   */
#define rb_dequeue(rb, out_ptr, ok_ptr)                                     \
do {                                                                        \
    bool _ok = false;                                                       \
    size_t pos = atomic_load_explicit(&(rb)->head, memory_order_relaxed);   \
    while (1) {                                                             \
        size_t idx = pos % rb_len(rb); /* or pos & (N-1) if power-of-two */ \
        size_t seq = atomic_load_explicit(&(rb)->items[idx].sequence,       \
                                          memory_order_acquire);            \
        intptr_t dif = (intptr_t)seq - (intptr_t)(pos + 1);                 \
        if (dif == 0) {                                                     \
            if (atomic_compare_exchange_weak_explicit(                      \
                    &(rb)->head, &pos, pos + 1,                             \
                    memory_order_relaxed, memory_order_relaxed)) {          \
                *(out_ptr) = (rb)->items[idx].value;                        \
                atomic_store_explicit(&(rb)->items[idx].sequence,           \
                                      pos + rb_len(rb),                     \
                                      memory_order_release);                \
                _ok = true;                                                 \
                break;                                                      \
            }                                                               \
        } else if (dif < 0) {                                               \
            /* empty */                                                     \
            _ok = false;                                                    \
            break;                                                          \
        } else {                                                            \
            pos = atomic_load_explicit(&(rb)->head, memory_order_relaxed);  \
        }                                                                   \
    }                                                                       \
    *(ok_ptr) = _ok;                                                        \
} while (0)

#define RB_DEQUEUE_SPINLOCK(rb, out_ptr)                                  \
do {                                                                      \
    bool _ok;                                                             \
    do {                                                                  \
        RB_DEQUEUE((rb), (out_ptr), &_ok);                                \
    } while (!_ok);                                                       \
    (void)_ok;                                                            \
} while (0)

