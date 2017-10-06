#include "mem_alloc.h"
#include <mm/mm.h>
#include <aos/paging.h>

static void mm_alloc_100f(void){
    TEST_PRINT_INFO("allocate 100 frames (sizes 30 + 100*i) \n This will also demonstrate slab refills");
    
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
    
    TEST_PRINT_SUCCESS();
}

static void mm_alloc_and_map_10f(void){
    TEST_PRINT_INFO("allocate 10 frames of different sizes and map them");
    
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
        err = paging_map_frame_attr(get_current_paging_state(), &buf, bytes, frame[i],VREGION_FLAGS_READ_WRITE, NULL, NULL);
        if (err_is_fail(err)) {
            TEST_PRINT_FAIL();
        }
    }
    
    TEST_PRINT_SUCCESS();
}

//TODO: alloc more than 256

static void mm_alloc_free_20(void){
    TEST_PRINT_INFO("allocate 20 chunks of ram (sizes 30 + 500*i). Repeated twice");
    
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

static void mm_alloc_free_10(void){
    TEST_PRINT_INFO("allocate 10 chunks of ram (sizes 30 + 1000*i). Free every second. allocate 10 chunks of different size again and free all");
    
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



static void mm_tests_run(void){
    mm_alloc_and_map_10f();

    mm_alloc_free_20();
    mm_alloc_free_10();
    mm_alloc_100f();
}

//    for(int i = 1; i<3; ++i){
//        struct capref frame;
//        size_t frame_size=0;
//        size_t bytes = 3000 * i;
//        err = frame_alloc(&frame, bytes, &frame_size);
//        debug_printf("Allocating a frame with %u bytes results in %u byte frame\n", bytes, frame_size);
//        if (err_is_fail(err)) {
//            return err;
//        }
//
//        void* buf;
//        err = paging_map_frame_attr(get_current_paging_state(), &buf, bytes, frame,VREGION_FLAGS_READ_WRITE, NULL, NULL);
//        if (err_is_fail(err)) {
//            debug_printf("error while refilling slab %s", err_getstring(err));
//            return err;
//        }
//    }
//
//    for (int i = 0; i < 300; i++) {
//        struct capref frame;
//        mm_alloc(&aos_mm, BASE_PAGE_SIZE, &frame);
//        if (i > 0 && i % 50 == 0) {
//            printf("Allocated %i chunk of size %u\n", i, BASE_PAGE_SIZE);
//        }
//    }
//
//   // USER_PANIC("HERE");
//
//
//    for (int i = 0; i < 300; i++) {
//        struct capref frame;
//        mm_alloc(&aos_mm, BASE_PAGE_SIZE, &frame);
//        mm_free(&aos_mm, frame, 0, 0);
//        if (i > 0 && i % 50 == 0) {
//            printf("Allocated and freed %i chunk of size %u\n", i, BASE_PAGE_SIZE);
//        }
//    }
