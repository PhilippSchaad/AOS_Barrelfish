#include "mem_alloc.h"
#include <mm/mm.h>
#include <aos/paging.h>

__attribute__((unused))
static int mm_alloc_300f(void)
{
    TEST_PRINT_INFO("Allocate 300 frames (sizes 30 + 100*i).\n"  \
            "           "   \
            "This will also demonstrate slab refills.");

    errval_t err;

    struct capref frame[300];

    for(int i = 0; i<300; ++i){
        size_t frame_size=0;
        size_t bytes = 30 + i*100 ;
//        debug_printf("round: %i \n", i);
        err = frame_alloc(&frame[i], bytes, &frame_size);
        if (err_is_fail(err)) {
            TEST_PRINT_FAIL();
        }
    }

    for(int i = 0; i<300; ++i){
        size_t bytes = 30 + i*100;

        err = aos_ram_free(frame[i], bytes);
        cap_destroy(frame[i]);
        if (err_is_fail(err)) {
            TEST_PRINT_FAIL();
        }
    }

    TEST_PRINT_SUCCESS();
}

__attribute__((unused))
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
        err = paging_map_frame_attr(get_current_paging_state(), &buf, frame_size,
                                    frame[i],VREGION_FLAGS_READ_WRITE, NULL,
                                    NULL);
        char* access = (char*)buf;
        for(int j = 0; j < frame_size; j++) {
            access[j] = 'i';
        }
        for(int j = 0; j < frame_size; j++) {
            if(access[j] != 'i') {
                debug_printf("access[%i]=='i' failed", j);
                TEST_PRINT_FAIL();
            }
        }


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

__attribute__((unused))
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
        err = paging_map_frame_attr(get_current_paging_state(), &buf, frame_size,
                                    frame[i],VREGION_FLAGS_READ_WRITE, NULL,
                                    NULL);

        char* access = (char*)buf;
        for(int j = 0; j < frame_size; j++) {
            access[j] = 'i';
        }
        for(int j = 0; j < frame_size; j++) {
            if(access[j] != 'i') {
                debug_printf("access[%i]=='i' failed", j);
                TEST_PRINT_FAIL();
            }
        }

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

__attribute__((unused))
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

__attribute__((unused))
static int mm_alloc_free_600(void)
{
    TEST_PRINT_INFO("Allocate 600 chunks of ram (size 30) and free again. This also uses slot alloc.");

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

__attribute__((unused))
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

__attribute__((unused))
static int mm_paging_map_fixed_attr_cursize_test(void) {
    TEST_PRINT_INFO("Regression test for issue #21");
    struct capref frame;
    errval_t err;
    size_t frame_size2=0;
    err = frame_alloc(&frame, (size_t)278528, &frame_size2);
    if (err_is_fail(err)) {
        TEST_PRINT_FAIL();
    }
    paging_map_fixed_attr(get_current_paging_state(),0x55c2000,frame,frame_size2,VREGION_FLAGS_READ_WRITE);
    err = aos_ram_free(frame, frame_size2);
    //cap_destroy(frame);
    TEST_PRINT_SUCCESS();
}

__attribute__((unused))
static int mm_paging_alloc_aligned_allignment_test(void) {
    TEST_PRINT_INFO("Regression test for alignment issue");
    errval_t err;
    struct capref frame[3];
    size_t sizes[3];
    sizes[0] = 4096 + 1024;
    sizes[1] = 4096 * 3 + 1024;
    sizes[2] = 4096 * 3 + 1024;
    //we get it out of alignment
    err = aos_ram_alloc_aligned(&frame[0], sizes[0], BASE_PAGE_SIZE);
    if (err_is_fail(err)) {
        TEST_PRINT_FAIL();
    }
    //we alloc our first test thing
    err = aos_ram_alloc_aligned(&frame[1], sizes[1], BASE_PAGE_SIZE * 4);
    if (err_is_fail(err)) {
        TEST_PRINT_FAIL();
    }
    //we alloc our second test thing, in case the very first alloc got us into alignment by random chance
    err = aos_ram_alloc_aligned(&frame[2], sizes[2], BASE_PAGE_SIZE * 4);
    if (err_is_fail(err)) {
        TEST_PRINT_FAIL();
    }

    for (int i = 1; i < 3; i++) {
        struct frame_identity ret;
        err = frame_identify(frame[i], &ret);
        if (err_is_fail(err)) {
            TEST_PRINT_FAIL();
        }
        if ((ret.base % (BASE_PAGE_SIZE * 4)) != 0) {
            TEST_PRINT_FAIL();
        }
    }

    for(int i = 0; i<3; ++i){
        err = aos_ram_free(frame[i], sizes[i]);
        if (err_is_fail(err)) {
            TEST_PRINT_FAIL();
        }
    }

    TEST_PRINT_SUCCESS();
}
