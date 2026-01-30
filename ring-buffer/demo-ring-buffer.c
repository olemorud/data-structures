
#include <stdio.h>
#include <stdlib.h>

#include "ring-buffer-int-128.h"

int main()
{
    struct rb_int_128 rb;
    rb_int_128_init(&rb);

    bool ok = rb_int_128_enqueue(&rb, 5);

    if (!ok) {
        // rb is full
        return EXIT_FAILURE;
    }

    int v;
    ok = rb_int_128_dequeue(&rb, &v);
    if (!ok) {
        // rb is empty
        return EXIT_FAILURE;
    }

    // should print "5"
    printf("%d", v);

    return EXIT_SUCCESS;
}
