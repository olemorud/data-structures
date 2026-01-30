/*
 * C Macro templated version of Dmitry Vyukov’s Multi-Producer Multi-Consumer
 * queue (DV-MPMC), by Ole Morud.
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
 *  // defines `struct rb_int_128`, a ring buffer of size 128 with type int.
 *  // optionally set RB_TAG to change the struct tag and function prefixes
 *  #define RB_T int
 *  #define RB_SIZE 128
 *  #include "ring-buffer-2.h"
 *
 *  int main()
 *  {
 *      struct rb_int_128 rb;
 *      rb_int_128_init(&rb);
 *
 *      rb_int_128_enqueue(&rb, 5);
 *
 *      if (!ok) {
 *          // rb is full
 *          return EXIT_FAILURE;
 *      }
 *
 *      int v;
 *      bool ok = rb_dequeue(&rb, &v);
 *      if (!ok) {
 *          // rb is empty
 *          return EXIT_FAILURE;
 *      }
 *
*       // should print "5"
 *      printf("%d", v);
 *
 *      return EXIT_SUCCESS;
 *  }
 *  ```
 */

#ifndef RB_T
#error "Define RB_T before including" __FILE__
#endif

#ifndef RB_SIZE
#error "Define RB_SIZE before including" __FILE__
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdatomic.h>

#define RB_XCAT3(a,b,c)   a##b##c
#define RB_XCAT4(a,b,c,d) a##b##c##d
#define RB_CAT3(a,b,c)    RB_XCAT3(a,b,c)
#define RB_CAT4(a,b,c,d)  RB_XCAT4(a,b,c,d)

#ifndef RB_TAG
// generally produces something like rb_int_128
//#define RB_TAG rb_##RB_X(RB_T)##RB_X(RB_SIZE)
#define RB_TAG RB_CAT4(rb_,RB_T,_,RB_SIZE)
#endif

#define RB_ID(id) RB_CAT3(RB_TAG,_,id)

typedef struct RB_TAG {
    atomic_size_t head;
    atomic_size_t tail;
    struct {
        RB_T          value;
        atomic_size_t sequence;
    } items[RB_SIZE];
} RB_TAG;

void RB_ID(init)(struct RB_TAG* rb);

bool RB_ID(enqueue)(struct RB_TAG* rb, RB_T v);

bool RB_ID(dequeue)(struct RB_TAG* rb, RB_T* out);

#ifndef RB_HEADER_ONLY

void RB_ID(init)(struct RB_TAG* rb)
{
    for (size_t i = 0; i < RB_SIZE; ++i) {
        atomic_store_explicit(&rb->items[i].sequence, i, memory_order_relaxed);
    }
    atomic_store_explicit(&rb->head, 0, memory_order_relaxed);
    atomic_store_explicit(&rb->tail, 0, memory_order_relaxed);
}

/*
    rb_enqueue
    rb     - ring buffer to insert to
    v      - inserted value 
    ok_ptr - underlying value set to true if insertion succeeded,
             false if ring buffer is full
   */
bool RB_ID(enqueue)(struct RB_TAG* rb, RB_T v)
{
    bool ok = false;
    size_t pos = atomic_load_explicit(&rb->tail, memory_order_relaxed);
    while (1) {
        size_t idx = pos % RB_SIZE;
        size_t seq = atomic_load_explicit(&rb->items[idx].sequence,
                                          memory_order_acquire);
        intptr_t dif = (intptr_t)seq - (intptr_t)pos;
        if (dif == 0) {
            if (atomic_compare_exchange_weak_explicit(
                    &rb->tail,
                    &pos,
                    pos + 1,
                    memory_order_relaxed,
                    memory_order_relaxed))
            {
                rb->items[idx].value = v;
                atomic_store_explicit(&rb->items[idx].sequence,
                                      pos + 1,
                                      memory_order_release);
                ok = true;
                break;
            }
        }
        /* CAS failed: pos updated, retry */
        else if (dif < 0) {
            /* full */
            ok = false;
            break;
        
        } 
        /* contention: move to current tail */
        else {
            pos = atomic_load_explicit(&rb->tail, memory_order_relaxed);
        }
    }
    return ok;
}

/*
    RB_DEQUEUE
    rb      - ring buffer to fetch from
    out_ptr - fetched value 
    ok_ptr  - underlying value set to true if dequeue succeeded,
              false if ring buffer is empty
   */
bool RB_ID(dequeue)(struct RB_TAG* rb, RB_T* out)
{
    bool ok = false;
    size_t pos = atomic_load_explicit(&rb->head, memory_order_relaxed);
    while (1) {
        size_t idx = pos % RB_SIZE; /* or pos & (N-1) if power-of-two */
        size_t seq = atomic_load_explicit(&rb->items[idx].sequence,
                                          memory_order_acquire);
        intptr_t dif = (intptr_t)seq - (intptr_t)(pos + 1);
        if (dif == 0) {
            if (atomic_compare_exchange_weak_explicit(
                    &rb->head,
                    &pos, 
                    pos + 1,
                    memory_order_relaxed,
                    memory_order_relaxed))
            {
                *out = rb->items[idx].value;
                atomic_store_explicit(&rb->items[idx].sequence,
                                      pos + RB_SIZE,
                                      memory_order_release);
                ok = true;
                break;
            }
        }
        /* empty */
        else if (dif < 0) {
            ok = false;
            break;
        }
        else {
            pos = atomic_load_explicit(&rb->head, memory_order_relaxed);
        }
    }
    return ok;
}

RB_T RB_ID(dequeue_spinlock)(struct RB_TAG* rb)
{
    bool ok;
    RB_T ret;
    do {
        ok = RB_ID(dequeue)(rb, &ret);
    } while (!ok);
    return ret;
}

#endif
