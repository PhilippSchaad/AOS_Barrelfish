#include <aos/aos.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <aos/aos_rpc.h>
#include <aos/caddr.h>
#include <aos/cspace.h>
#include <aos/sys_debug.h>
#include <math.h>

#include <arch/arm/barrelfish_kpi/asm_inlines_arch.h>

#define TEST_PRINT(x ...) \
    printf("<grading> " x);


#define BUFSIZE (64UL*1024*1024)

struct aos_chan **conns = NULL;
size_t conns_len = 0;

struct consume_state {
    size_t bufsize;
    char c;
};

static int consume(void *arg)
{
    struct consume_state *st = arg;
    size_t bufsize = st->bufsize;

    debug_printf("filling %u MB\n", bufsize / 1024 / 1024);
    unsigned char *buf = malloc(bufsize);
    if (!buf) {
        debug_printf("OOM!\n");
        return 1;
    }
    assert(buf);
    debug_printf("buf = %p\n", buf);

    const char c = st->c;

    for (int i=0; i<bufsize; i++) {
        buf[i] = i % 256;
        if (i % (1UL<<20) == 0) {
            debug_printf("[%c] %d MB written\n", c, i / (1UL<<20));
        }
    }
    sys_debug_flush_cache();
    debug_printf("checking buffer %p\n", buf);
    for (int i=0; i<bufsize; i++) {
        if (buf[i] != i % 256) {
            debug_printf("failure at byte 0x%x\n", i);
        }
        assert(buf[i] == i % 256);
        if (i % (1UL<<20) == 0) {
            debug_printf("[%c] %d MB checked\n", c, i / (1UL<<20));
        }
    }

    free(buf);
    return EXIT_SUCCESS;
}


#define TEST_MULTIPLE_NPAGES (16)
#define TEST_MULTIPLE_SIZE (TEST_MULTIPLE_NPAGES * BASE_PAGE_SIZE)


void *addrs[TEST_MULTIPLE_NPAGES];

static void test_single_pagefault(void *addr)
{
    printf("Faulting on %p...", addr);

    uint32_t *target = addr;

    for (size_t i = 0; i < BASE_PAGE_SIZE / sizeof(uint32_t); i++) {
        target[i] = (uint32_t)(uintptr_t)addr;
    }

    for (size_t i = 0; i < BASE_PAGE_SIZE / sizeof(uint32_t); i++) {
        if (target[i] != (uint32_t)(uintptr_t)addr) {
            printf("Verification FAILED\n");
        }
    }

    printf("OK\n");
}


static void test_malloc(size_t bytes)
{
    TEST_PRINT("test_malloc: Allocating buffer of size %zu...\n", bytes);

    uint32_t *addr = malloc(bytes);
    if (!addr) {
        TEST_PRINT("test_malloc: FAILURE. Didn't get a buffer\n");
    }

    test_single_pagefault(addr);

    free(addr);

    TEST_PRINT("test_malloc: OK.\n");
}

#if 0
void test_unmap(void)
{

}

void test_map_fixed(void)
{

}
#endif


static void test_fault(void)
{
    uint32_t *addr = (uint32_t *)0x38000000;
    TEST_PRINT("starting test. Expected to fault at address %p\n", addr);

    *addr = 0xcafebabe;

    TEST_PRINT("FAILURE. Continue execution after expected fault\n");
}


#define NTHREADS 4
static void test_multithreaded(void)
{
    for (int t = 1; t <= NTHREADS; t*=2) {
        TEST_PRINT("starting test. NTHREADS = %u\n", NTHREADS);
        struct thread *threads[t];
        for (int k = 0; k < t; k++) {
            struct consume_state *st = malloc(sizeof(struct consume_state));
            st->bufsize =  BUFSIZE;
            st->c = 0x30+k;
            threads[k] = thread_create(consume, st);
        }
        for (int k = 0; k < t; k++) {
            int i;
            errval_t err = thread_join(threads[k], &i);
            assert(err_is_ok(err));
            if (i) {
                debug_printf("exit failure for thread %d\n", k);
            }
        }

        TEST_PRINT("OK.\n");
    }

}

#define TEST_MALLOC_SINGLE (4096)
#define TEST_MALLOC_MULTIPLE (8*4096)
#define TEST_MALLOC_SMALL (32UL * 1024 * 1024)
#define TEST_MALLOC_LARGE (400UL * 1024 * 1024)

int main(int argc, char *argv[])
{
    debug_printf("memtest started\n");

    test_malloc(TEST_MALLOC_SMALL);
    test_malloc(TEST_MALLOC_LARGE);
    test_malloc(TEST_MALLOC_SINGLE);
    test_malloc(TEST_MALLOC_MULTIPLE);
    test_multithreaded();
    test_fault();

    debug_printf("memtest done\n");

    return 0;
}
