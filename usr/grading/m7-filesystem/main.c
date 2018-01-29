/**
 * \file
 * \brief Hello world application
 */

/*
 * Copyright (c) 2016 ETH Zurich.
 * All rights reserved.
 *
 * This file is distributed under the terms in the attached LICENSE file.
 * If you do not find this file, copies can be found by writing to:
 * ETH Zurich D-INFK, CAB F.78, Universitaetstr. 6, CH-8092 Zurich,
 * Attn: Systems Group.
 */


#include <stdio.h>
#include <ctype.h>
#include <aos/aos.h>
#include <fs/fs.h>
#include <fs/dirent.h>
#include <omap_timer/timer.h>

#define ENABLE_LONG_FILENAME_TEST 1
#define ENABLE_WRITE_TEST 0

/* reading */
#define MOUNTPOINT     "/sdcard"
#define SUBDIR         "/parent"
#define SUBDIRSUB      "/parent/sub"
#define SUBDIR_LONG    "/parent-directory"
#define DIR_NOT_EXIST  "/not-exist"
#define DIR_NOT_EXIST2 "/parent/not-exist"
#define FILENAME       "/myfile.txt"
#define FILENAME_DIR   "/parent/myfile.txt"
#define FILENAME_SUBDIR "/parent/sub/myfile.txt"
#define LONGFILENAME   "/mylongfilenamefile.txt"
#define LONGFILENAME2  "/mylongfilenamefilesecond.txt"
#define FILE_NOT_EXIST "/not-exist.txt"
#define FILE_NOT_EXIST2 "/parent/not-exist.txt"
#define LARGE_FILE      "/large.dat"
#define HUGE_FILE      "/huge.dat"
#define ENORMOUS_FILE  "/enormous.dat"


#define TEST_PREAMBLE(arg) \
    printf("\n-------------------------------\n"); \
    printf("%s(%s)\n", __FUNCTION__, arg);

#define TEST_END \
    printf("-------------------------------\n");

#define EXPECT_SUCCESS(err, test, _time) \
    do{ \
        if(err_is_fail(err)){ \
            DEBUG_ERR(err, test); \
        } else { \
            printf("SUCCESS: " test " took %" PRIu64 "\n", _time);\
        }\
   } while(0);\

#define EXPECT_FAILURE(err, _test, _time) \
    do{ \
        if(err_is_fail(err)){ \
            printf("SUCCESS: failure expected " _test " took %" PRIu64 " ms\n", _time);\
        } else { \
            DEBUG_ERR(err, "FAILURE: failure expected, but test succeeded" _test); \
        }\
   } while(0);\

#define run_test(fn, arg) \
    do { \
        tstart = omap_timer_read(); \
        err = fn(arg); \
        tend = omap_timer_read(); \
        EXPECT_SUCCESS(err, #fn, omap_timer_to_ms(tend-tstart)); \
        TEST_END \
    } while(0); \

#define run_test_fail(fn, arg) \
    do { \
        tstart = omap_timer_read(); \
        err = fn(arg); \
        tend = omap_timer_read(); \
        EXPECT_FAILURE(err, #fn, omap_timer_to_ms(tend-tstart)); \
        TEST_END \
    } while(0); \


static errval_t test_read_dir(char *dir)
{
    errval_t err;

    TEST_PREAMBLE(dir)

    fs_dirhandle_t dh;
    err = opendir(dir, &dh);
    if (err_is_fail(err)) {
        return err;
    }

    assert(dh);

    do {
        char *name;
        err = readdir(dh, &name);
        if (err_no(err) == FS_ERR_INDEX_BOUNDS) {
            break;
        } else if (err_is_fail(err)) {
            goto err_out;
        }
        printf("%s\n", name);
    } while(err_is_ok(err));

    return closedir(dh);
    err_out:
    return err;
}

static char test_fread_buf[128];

static errval_t test_fread(char *file)
{
    int res = 0;

    TEST_PREAMBLE(file)

    snprintf(test_fread_buf, sizeof(test_fread_buf), "hello world! %s\n", file);

    FILE *f = fopen(file, "r");
    if (f == NULL) {
        return FS_ERR_OPEN;
    }

    /* obtain the file size */
    res = fseek (f , 0 , SEEK_END);
    if (res) {
        return FS_ERR_INVALID_FH;
    }

    size_t filesize = ftell (f);
    rewind (f);

    printf("File size is %zu\n", filesize);

    char *buf = calloc(filesize + 2, sizeof(char));
    if (buf == NULL) {
        return LIB_ERR_MALLOC_FAIL;
    }

    size_t read = fread(buf, 1, filesize, f);

    int correct = strncmp(buf, test_fread_buf, strlen(test_fread_buf));
    printf("verify: %s\n", (correct ? "OK" : "ERROR"));

    if (read != filesize || !correct) {
        printf("read: %s\n", buf);
        return FS_ERR_READ;
    }

    rewind(f);

    size_t nchars = 0;
    int c;
    do {
        c = fgetc (f);
        nchars++;
    } while (c != EOF);

    if (nchars < filesize) {
        return FS_ERR_READ;
    }

    free(buf);
    res = fclose(f);
    if (res) {
        return FS_ERR_CLOSE;
    }

    return SYS_ERR_OK;
}

static errval_t test_fread_large(char *file)
{
    int res = 0;

    TEST_PREAMBLE(file)

    FILE *f = fopen(file, "r");
    if (f == NULL) {
        return FS_ERR_OPEN;
    }

    /* obtain the file size */
    res = fseek (f , 0 , SEEK_END);
    if (res) {
        return FS_ERR_INVALID_FH;
    }

    size_t filesize = ftell (f);
    rewind (f);

    printf("File size is %zu\n", filesize);

    char *buf = calloc(filesize + 2, sizeof(char));
    if (buf == NULL) {
        return LIB_ERR_MALLOC_FAIL;
    }

    size_t read = fread(buf, 1, filesize, f);

    if (read != filesize) {
        return FS_ERR_READ;
    }

    size_t num_chars = atoi(buf);

    printf("File has %u 'A'\n", num_chars);

    size_t count = 0;
    for (int i = 0; i < filesize; ++i) {
        /* skip leading part */
        int code = buf[i];
        if (isdigit(code) || buf[i] == ' ' || buf[i] == '\n') {
            continue;
        }
        if (buf[i] == 'A') {
            count++;
            continue;
        }
        break;
    }

    if (num_chars != count) {
        printf("File read %zu 'A' expected %zu\n", count, num_chars);
        return FS_ERR_READ;
    }

    free(buf);
    res = fclose(f);
    if (res) {
        return FS_ERR_CLOSE;
    }

    return SYS_ERR_OK;
}

#if ENABLE_WRITE_TEST

static errval_t test_write_dir(char *dir)
{
    errval_t err;

    TEST_PREAMBLE(dir)

    printf("mkdir(%s)..", dir);
    err = mkdir(dir);
    if (err_is_fail(err)) {
        printf("FAIL\n");
        return err;
    }
    printf("OK.\n");

    char paths[260];

    /* create */
    for (int i = 0; i < 10; i++) {
        snprintf(paths, 260, "%s/dir%d", dir, i);
        printf("mkdir(%s) ...", paths);
        err = mkdir(paths);
        if (err_is_fail(err)) {
            printf("FAIL.\n");
            return err;
        }
        printf("OK.\n");

        for (int j = 0; j < 10; j++) {
            snprintf(paths, 260, "%s/dir%d/subdir%d", dir, i, j);
            err = mkdir(paths);
            if (err_is_fail(err)) {
                printf("mkdir(%s)...FAIL\n", paths);
                return err;
            }
        }
    }

    printf("opendir(%s)...", dir);
    fs_dirhandle_t dh;
    err = opendir(dir, &dh);
    if (err_is_fail(err)) {
        printf("FAIL.\n");
        return err;
    }
    printf("OK.\n");

    assert(dh);

    char *name;

    do {
        err = readdir(dh, &name);
        if (err_no(err) == FS_ERR_INDEX_BOUNDS) {
            break;
        } else if (err_is_fail(err)) {
            closedir(dh);
            return err;
        }

        if (name[0] == '.' && (name[1] == 0 || (name[1] == '.' && name[2] == 0))) {
            //continue;
        }

        printf("%s: \n > ", name);

        snprintf(paths, 260, "%s/%s", dir, name);

        errval_t err2;

        fs_dirhandle_t dh2;
        err2 = opendir(paths, &dh2);
        if (err_is_fail(err2)) {
            printf("opendir(%s)... FAIL.\n", paths);
            closedir(dh);
            return err2;
        }

        do {
            char *name2;
            err2 = readdir(dh2, &name2);
            if (err_no(err2) == FS_ERR_INDEX_BOUNDS) {
                break;
            } else if (err_is_fail(err2)) {
                closedir(dh2);
                closedir(dh);
                return err;
            }
            printf("%s ", name2);

        } while(err_is_ok(err2));

        printf("\n");
        closedir(dh2);

    } while(err_is_ok(err));

    closedir(dh);

    /* delete */
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 10; j++) {
            snprintf(paths, 260, "%s/dir%d/subdir%d", dir, i, j);
            err = rmdir(paths);
            if (err_is_fail(err)) {
                printf("rmdir(%s)...FAIL\n", paths);
                return err;
            }
        }

        snprintf(paths, 260, "%s/dir%d", dir, i);

        printf("rmdir(%s)...", paths);
        err = rmdir(paths);
        if (err_is_fail(err)) {
            printf("FAIL\n");
            return err;
        }
        printf("OK\n");
    }

    printf("rmdir(%s)...", dir);
    err = rmdir(dir);
    if (err_is_fail(err)) {
        printf("FAIL\n");
        return err;
    }
    printf("OK\n");

    return SYS_ERR_OK;
}



#define MAX_BUF_SIZE 8000
char databuf[MAX_BUF_SIZE];

static errval_t test_fwrite_common(char *file, size_t bytes)
{
    FILE *f = fopen(file, "w");
    if (f == NULL) {
        return FS_ERR_OPEN;
    }

    assert(bytes <= MAX_BUF_SIZE);

    uint32_t *ptr = (uint32_t*)databuf;
    for (int i = 0; i < bytes / sizeof(uint32_t); i++) {
        ptr[i] = bytes + i;
    }

    printf("writing buffer...");
    size_t written = 0;
    do {
        written += fwrite(databuf + written, 1, bytes - written, f);
    } while(written < bytes);
    printf("OK.\n");

    fclose(f);

    f = fopen(file, "r");
    if (f == NULL) {
        return FS_ERR_OPEN;
    }

    memset(databuf, 0x00, MAX_BUF_SIZE);

    printf("reading buffer...");
    size_t read = 0;
    do {
        read += fread(databuf + read, 1, bytes - read, f);
    } while(read < bytes);
    printf("OK.\n");

    /* verify */
    printf("verify buffer... ");
    for (int i = 0; i < bytes / sizeof(uint32_t); i++) {
        if (ptr[i] != bytes + i) {
            printf("position: %u not the same %x\n", i, ptr[i]);
            return -1;
        }
    }
    printf("OK.\n");

    return SYS_ERR_OK;
}


static errval_t test_fwrite(char *file)
{
    TEST_PREAMBLE(file)

    return test_fwrite_common(file, 250);
}

static errval_t test_fwrite_large(char *file)
{
    TEST_PREAMBLE(file)

    return test_fwrite_common(file, MAX_BUF_SIZE);
}
#endif

int main(int argc, char *argv[])
{
    errval_t err;
    uint64_t tstart, tend;

    printf("Filereader test\n");

    err = omap_timer_init();
    assert(err_is_ok(err));

    printf("init timer\n");
    omap_timer_ctrl(true);

    printf("initializing filesystem...\n");
    err = filesystem_init();
    EXPECT_SUCCESS(err, "foobar", 0);

    err = filesystem_mount("/sdcard", "mmchs://fat32/0");
    EXPECT_SUCCESS(err, "foobar", 0);

    run_test(test_read_dir, MOUNTPOINT "/");

    run_test(test_read_dir, MOUNTPOINT SUBDIR);

    run_test(test_read_dir, MOUNTPOINT SUBDIRSUB);

    run_test_fail(test_read_dir, DIR_NOT_EXIST);

    run_test_fail(test_read_dir, DIR_NOT_EXIST2);

    run_test(test_fread, MOUNTPOINT FILENAME);

    run_test(test_fread, MOUNTPOINT FILENAME_DIR);

    run_test(test_fread, MOUNTPOINT FILENAME_SUBDIR);

    run_test(test_fread_large, MOUNTPOINT LARGE_FILE);

    run_test(test_fread_large, MOUNTPOINT HUGE_FILE);


#if ENABLE_LONG_FILENAME_TEST
    run_test(test_fread, MOUNTPOINT LONGFILENAME);

    run_test(test_fread, MOUNTPOINT LONGFILENAME2);
#endif

    run_test_fail(test_fread, MOUNTPOINT FILE_NOT_EXIST);

    run_test_fail(test_fread, MOUNTPOINT FILE_NOT_EXIST2);

    /* write tests */
#if ENABLE_WRITE_TEST
    run_test(test_write_dir, MOUNTPOINT "/testdir");

    run_test(test_fwrite, MOUNTPOINT "/testf.dat");

    run_test(test_fwrite_large, MOUNTPOINT "/testfile_large.dat");
#endif
    return EXIT_SUCCESS;
}
