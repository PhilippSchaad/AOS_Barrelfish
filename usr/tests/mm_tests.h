#include "mem_alloc.h"
#include <mm/mm.h>
#include <aos/paging.h>

static int mm_alloc_100f(void)
{
    TEST_PRINT_INFO("Allocate 100 frames (sizes 30 + 100*i).\n"  \
            "           "   \
            "This will also demonstrate slab refills.");

    errval_t err;

    struct capref frame[100];

    for(int i = 0; i<100; ++i){
        size_t frame_size=0;
        size_t bytes = 30 + i*100 ;
        err = frame_alloc(&frame[i], bytes, &frame_size);
        if (err_is_fail(err)) {
            TEST_PRINT_FAIL();
        }
    }

    for(int i = 0; i<100; ++i){
        size_t bytes = 30 + i*100;

        err = aos_ram_free(frame[i], bytes);
        cap_destroy(frame[i]);
        if (err_is_fail(err)) {
            TEST_PRINT_FAIL();
        }
    }

    TEST_PRINT_SUCCESS();
}

static int mm_alloc_and_map_10f(void)
{
    TEST_PRINT_INFO("Allocate 10 frames of different sizes and map them.");

    errval_t err;

    struct capref frame[10];

    for(int i = 0; i<10; ++i){
        size_t frame_size=0;
        size_t bytes = 30 + i*1000 ;
        err = frame_alloc(&frame[i], bytes, &frame_size);
        if (err_is_fail(err)) {
            TEST_PRINT_FAIL();
        }
        void *buf;
        err = paging_map_frame_attr(get_current_paging_state(), &buf, bytes,
                                    frame[i],VREGION_FLAGS_READ_WRITE, NULL,
                                    NULL);
        if (err_is_fail(err)) {
            TEST_PRINT_FAIL();
        }
    }

    for(int i = 0; i<10; ++i){
        size_t bytes = 30 + i*1000 ;
        err = aos_ram_free(frame[i], bytes);
        cap_destroy(frame[i]);
        if (err_is_fail(err)) {
            TEST_PRINT_FAIL();
        }
    }

    TEST_PRINT_SUCCESS();
}

static int mm_alloc_and_map_large_10f(void)
{
    TEST_PRINT_INFO("Allocate 10 large frames of different sizes and map them.");

    errval_t err;

    struct capref frame[10];

    for(int i = 0; i<10; ++i){
        size_t frame_size=0;
        size_t bytes = 30 + i*(1<<20) ;
        err = frame_alloc(&frame[i], bytes, &frame_size);
        if (err_is_fail(err)) {
            TEST_PRINT_FAIL();
        }
        void *buf;
        err = paging_map_frame_attr(get_current_paging_state(), &buf, bytes,
                                    frame[i],VREGION_FLAGS_READ_WRITE, NULL,
                                    NULL);
        if (err_is_fail(err)) {
            TEST_PRINT_FAIL();
        }
    }

    for(int i = 0; i<10; ++i){
        size_t bytes = 30 + i*(1<<20) ;
        err = aos_ram_free(frame[i], bytes);
        cap_destroy(frame[i]);
        if (err_is_fail(err)) {
            TEST_PRINT_FAIL();
        }
    }

    TEST_PRINT_SUCCESS();
}

static int mm_alloc_free_20(void)
{
    TEST_PRINT_INFO("Allocate 20 chunks of ram (sizes 30 + 500*i). Repeated twice.");

    errval_t err;
    
    for (int j=0; j<2;++j) {
        struct capref frame[20];

        for(int i = 0; i<20; ++i){
            size_t bytes = 30 + i*500 ;
            err = aos_ram_alloc_aligned(&frame[i], bytes, BASE_PAGE_SIZE);
            if (err_is_fail(err)) {
                TEST_PRINT_FAIL();
            }
        }

        for(int i = 0; i<20; ++i){
            size_t bytes = 30 + i*500 ;
            err = aos_ram_free(frame[i], bytes);
            if (err_is_fail(err)) {
                TEST_PRINT_FAIL();
            }
        }
    }

    TEST_PRINT_SUCCESS();
}

static int mm_alloc_free_600(void)
{
    TEST_PRINT_INFO("Allocate 400 chunks of ram (size 30) and free again. This also uses slot alloc.");

    errval_t err;

    struct capref frame[600];

    for(int i = 0; i<600; ++i){
        size_t bytes = 30 ;
        err = aos_ram_alloc_aligned(&frame[i], bytes, BASE_PAGE_SIZE);
        if (err_is_fail(err)) {
            TEST_PRINT_FAIL();
        }
    }

    for(int i = 0; i<600; ++i){
        size_t bytes = 30 ;
        err = aos_ram_free(frame[i], bytes);
        if (err_is_fail(err)) {
            TEST_PRINT_FAIL();
        }
    }


    TEST_PRINT_SUCCESS();
}

static int mm_alloc_free_10(void)
{
    TEST_PRINT_INFO("Allocate 10 chunks of ram (sizes 30 + 1000*i). Free every second.\n"   \
            "           "   \
            "Allocate 10 chunks of different size again and free all.");

    errval_t err;

    for (int j=0; j<2;++j) {
        struct capref frame[15];
        size_t sizes[15];
        //alloc 10
        for(int i = 0; i<10; ++i){
            sizes[i] = 30 + i*1000 ;
            err = aos_ram_alloc_aligned(&frame[i], sizes[i], BASE_PAGE_SIZE);
            if (err_is_fail(err)) {
                TEST_PRINT_FAIL();
            }
        }
        //free 5 (every second)
        for(int i = 0; i<10; i+=2){
            err = aos_ram_free(frame[i], sizes[i]);
            if (err_is_fail(err)) {
                TEST_PRINT_FAIL();
            }
        }

        //alloc 5
        for(int i = 0; i<10; i+=2){
            sizes[i] = 10000 - i*1000 ;
            err = aos_ram_alloc_aligned(&frame[i], sizes[i], BASE_PAGE_SIZE);
            if (err_is_fail(err)) {
                TEST_PRINT_FAIL();
            }
        }

        //alloc 5
        for(int i = 10; i<15; ++i){
            sizes[i] = 30 + i*500 ;
            err = aos_ram_alloc_aligned(&frame[i], sizes[i], BASE_PAGE_SIZE);
            if (err_is_fail(err)) {
                TEST_PRINT_FAIL();
            }
        }

        // free 15
        for(int i = 0; i<15; ++i){
            err = aos_ram_free(frame[i], sizes[i]);
            if (err_is_fail(err)) {
                TEST_PRINT_FAIL();
            }
        }
    }

    TEST_PRINT_SUCCESS();
}
