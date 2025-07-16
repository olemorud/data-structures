
#include "critbit.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>

static void print_spaces(int n)
{
    for (int i = 0; i < n; i++) {
        putchar(' ');
    }
}

int main()
{
    setbuf(stdout, NULL);
    srand(0);

    const int LINE_WIDTH = 70;

    uint8_t data[1<<17];

    for (int i = 0; i < 2; i++) {
        struct critbit_tree cbt = {0};
        struct arena a = {0};
        switch (i) {
        case 0:
            printf("\n\n");
            printf("Testing with static arena\n");
            printf("=========================\n");
            a = arena_attach(data, sizeof data);
            cbt.arena = &a;
            break;
        case 1:
            printf("\n\n");
            printf("Testing with dynamic arena\n");
            printf("==========================\n");
            a = arena_new();
            cbt.arena = &a;
            break;
        default:
            assert(false);
        }

        const char* good_strings[] = {
            "hello world",
            "hello world tutorial",
            "hello sunshine",
            "hello hello hello",
            "hel",
            "he",
            "h",
            "asd",
        };
        const char* bad_strings[] = {
            "does not exist",
        };

        {
            const char* s = "asd";
            int n = printf("checking that \"%s\" is not in empty set", s);
            print_spaces(LINE_WIDTH - n);
            if (!critbit_contains(&cbt, s, strlen(s))) {
                printf("- OK!\n");
            }
        }

        for (size_t i = 0; i < sizeof good_strings / sizeof *good_strings; i++) {
            int n = printf("checking for \"%s\", which shouldn't be in set yet", good_strings[i]);
            print_spaces(LINE_WIDTH - n);
            if (critbit_contains(&cbt, good_strings[i], strlen(good_strings[i]))) {
                printf("- BAD: found it anyways!\n");
            } else {
                printf("- OK!\n");
            }

            n = printf("inserting %s", good_strings[i]);
            print_spaces(LINE_WIDTH - n);
            int ok = 0;
            if (ok = critbit_insert(&cbt, good_strings[i], strlen(good_strings[i]))) {
                printf("%i - BAD: got error message during critbit_insert\n", ok);
            } else {
                printf("- OK!\n");
            }
        }

        for (size_t i = 0; i < sizeof bad_strings / sizeof *bad_strings; i++) {
            int n = printf("checking for \"%s\", which shouldn't be in set yet", bad_strings[i]);
            print_spaces(LINE_WIDTH - n);
            if (!critbit_contains(&cbt, bad_strings[i], strlen(bad_strings[i]))) {
                printf("- OK!\n");
            } else {
                printf("- BAD: found %s when not supposed to be in set!\n", bad_strings[i]);
            }
        }

        {
            int random_insertions = 1000;
            int n = printf("adding, checking and removing %i random strings", random_insertions);
            print_spaces(LINE_WIDTH - n);
            for (size_t i = 0; i < random_insertions; i++) {
                size_t len = (i % (16)) + 16;
                char randstr[len];
                for (size_t i = 0; i+1 < len; i++) {
                    randstr[i] = 'A' + rand()%('Z'-'A');
                }
                randstr[len] = '\0';

                if (critbit_contains(&cbt, randstr, strlen(randstr))) {
                    printf("- BAD, found %s before it was added!\n", randstr);
                    goto fail;
                }
                int ok;
                if (ok = critbit_insert(&cbt, randstr, strlen(randstr))) {
                    printf("BAD: got error %i when trying to insert %s, after %zu iterations\n", ok, randstr, i);
                    goto fail;
                }
                if (!critbit_contains(&cbt, randstr, strlen(randstr))) {
                    printf("- BAD, didn't find %s after it was added, after %zu iterations!\n", randstr, i);
                    printf("Critbit tree:\n");
                    print_node_data(printf, cbt.root, 999, 0);
                    printf("\n");
                    goto fail;
                }
                if (ok = critbit_remove(&cbt, randstr, strlen(randstr))) {
                    printf("BAD: got error %i when trying to remove %s, after %zu iterations\n", ok, randstr, i);
                    printf("Critbit tree:\n");
                    print_node_data(printf, cbt.root, 999, 0);
                    printf("\n");
                    goto fail;
                }
                if (critbit_contains(&cbt, randstr, strlen(randstr))) {
                    printf("- BAD, found %s after it was removed, after %zu iterations!\n", randstr, i);
                    printf("Critbit tree:\n");
                    print_node_data(printf, cbt.root, 999, 0);
                    printf("\n");
                    goto fail;
                }
            }
            printf("- OK!\n");
fail:
        }

        for (size_t i = 0; i < sizeof good_strings / sizeof *good_strings; i++) {
            int n = printf("checking for \"%s\" again", good_strings[i]);
            print_spaces(LINE_WIDTH - n);
            if (critbit_contains(&cbt, good_strings[i], strlen(good_strings[i]))) {
                printf("- OK!\n");
            } else {
                printf("- BAD!\n");
            }
        }

        //print_node_data(printf, cbt.root, 999, 0);
        if (cbt.arena) {
            arena_delete(cbt.arena);
        }
    }

    return EXIT_SUCCESS;
}
