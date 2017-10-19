#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/time.h>
#include <utils/time.h>
#include <sos.h>
#include "file-system-unit-test.h"

#define mode_t uint32_t
#define MAX_FDNUM_PER_PROCESS 128
#define BUF_SIZE              1000000

#define NONE                 "\e[0m"
#define BLACK                "\e[0;30m"
#define L_BLACK              "\e[1;30m"
#define RED                  "\e[0;31m"
#define L_RED                "\e[1;31m"
#define GREEN                "\e[0;32m"
#define L_GREEN              "\e[1;32m"
#define BROWN                "\e[0;33m"
#define YELLOW               "\e[1;33m"
#define BLUE                 "\e[0;34m"
#define L_BLUE               "\e[1;34m"
#define PURPLE               "\e[0;35m"
#define L_PURPLE             "\e[1;35m"
#define CYAN                 "\e[0;36m"
#define L_CYAN               "\e[1;36m"
#define GRAY                 "\e[0;37m"
#define WHITE                "\e[1;37m"

#define BOLD                 "\e[1m"
#define UNDERLINE            "\e[4m"
#define BLINK                "\e[5m"
#define REVERSE              "\e[7m"
#define HIDE                 "\e[8m"
#define CLEAR                "\e[2J"
#define CLRLINE              "\r\e[K" //or "\e[1K\r"

// void file_unittest(void);
static int pass_count = 0;
static int total_count = 0;

static int fail_count = 0;

static char buf[BUF_SIZE] = {0};

/*// printf("errno : %d, msg: %s\n" NONE , errno, strerror(errno));\*/
#define TASSERT(expr, v) do {\
    if (!(expr)) \
    {\
        printf( CYAN "\n################ TASSERT ############\n ");\
        write(1, __FILE__, sizeof(__FILE__));\
        write(1, "(",1 );\
        printf("line: %d\n", (int)__LINE__);\
        write(1, "):",2 );\
        write(1, __func__, sizeof(__func__));\
        write(1, "--------->", 10);\
        write(1, #expr, sizeof(#expr)); \
        printf(", but value is %d  ", (int)v);\
        write(1, "\n--->", 5);\
        return -1;\
    }\
}while(0)

#define FUNCTION_CALL(func, args...) do{\
    int tmp = 0;\
    printf (YELLOW "\nrunning: %s\n" NONE, #func);\
    tmp = func(args);\
    (tmp == 0) ? (pass_count ++ ): (fail_count ++);\
    total_count ++;\
    if (tmp == 0) {\
        printf (GREEN "%s passed\n" NONE, #func);\
    } else {\
        printf (RED "%s failed\n" NONE, #func);\
    }\
} while(0)

#define BEGIN_FUNCTION {}
#define END_FUNCTION return 0

/*
*   No lseek function, so we have to close and open agian to
*   refresh the pointer
*/
static int update_internal_file_ptr(int fd, const char * filename, mode_t mode) {
    TASSERT(sos_sys_close(fd) == 0, 0);
    int fd_new = sos_sys_open(filename, mode);
    TASSERT(fd_new >= 0, fd_new);
    return fd_new;
}

// static int random_generator()
// {
//     time_t tt;
//     time(&tt);
//      printf ("time: %lld\n", tt);
//     srandom(tt);
//     /* printf ("random: %ld\n", random()); */
//     return (int)(random());

// }
// static char* random_str_generator()
// {
//     static char tmp[20] = {0};
//     int r = random_generator();
//     int len = snprintf(tmp, 19, "%d", r);
//     tmp[len] = 0;
//     return tmp;

// }
static int test_readonly()
{
    int ret =  0;
    int fd = sos_sys_open("tp_tr", O_RDONLY| O_CREAT);
    TASSERT(fd >= 0, fd);
    ret = sos_sys_read(fd, buf, 100);
    TASSERT(ret == 0, ret);

    fd = update_internal_file_ptr(fd, "tp_tr", O_RDONLY| O_CREAT);

    ret = sos_sys_write(fd, "w1", 2);
    TASSERT(ret < -1, ret);
    TASSERT(sos_sys_close(fd) == 0, 0);

    TASSERT(sos_sys_remove("tp_tr") == 0, 0);
    return 0;
}

static int test_writeonly()
{
    int ret =  0;
    int fd = sos_sys_open("tp_tw", O_WRONLY| O_CREAT);
    TASSERT(fd >= 0, fd);
    ret = sos_sys_write(fd, "test_writeonly", 14);
    TASSERT(ret == 14, 14);

    fd = update_internal_file_ptr(fd, "tp_tw", O_WRONLY| O_CREAT);

    ret = sos_sys_read(fd, buf, 14);
    TASSERT(ret < 0, ret);
    TASSERT(sos_sys_close(fd) == 0, 0);

    TASSERT(sos_sys_remove("tp_tw") == 0, 0);
    return 0;
}

static int test_readandwrite(void)
{
    int ret =  0;
    int fd = sos_sys_open("tp_raw", O_RDWR| O_CREAT);
    TASSERT(fd >= 0, fd);
    ret = sos_sys_write(fd, "test_readandwrite", 17);
    TASSERT(ret == 17, ret);

    fd = update_internal_file_ptr(fd, "tp_raw", O_RDWR| O_CREAT);

    ret = sos_sys_read(fd, buf, 17);
    TASSERT(ret == 17, ret);
    TASSERT(sos_sys_close(fd) == 0, 0);
    TASSERT(sos_sys_remove("tp_raw") == 0, 0);
    return 0;
}

static int test_invalid_sos_sys_close(void)
{
    BEGIN_FUNCTION;
    int ret = sos_sys_close(339);
    TASSERT(ret != 0, ret);
    ret = sos_sys_close(-1);
    TASSERT(ret != 0, ret);
    END_FUNCTION;
}

static int test_limited_open_fd(void)
{
    BEGIN_FUNCTION;

    int loop_times = 2;

    int how_many_fd_generate_each_time[loop_times];
    memset(how_many_fd_generate_each_time, 0, loop_times*sizeof(int) );

    for (int a = 0; a < loop_times; a++)
    {
        int fd_generate_counter = 0;
        int ret = 0;
        /*
         * 128 is the upper limit
         */
        int fd = 0 ;
        int upper_fd = 0;
        for (int i = 3; i < MAX_FDNUM_PER_PROCESS+1; i++)
        {
            char tmp[80] = {0};
            sprintf(tmp, "test_limited_open_fd_%d", i);
            fd = sos_sys_open(tmp, O_RDWR | O_CREAT);
            // printf("in open file loop, fd: %d\n", fd);
            if (fd >= 3)
            {
                /* printf("in open file loop, fd: %d, i: %d\n", fd,i); */
                TASSERT(fd == i, i);
                upper_fd = i;
                fd_generate_counter++;
            }
            else
            {
                printf ("open %d error!\n", i);
                TASSERT(fd == -24, fd);
                // TASSERT(errno, ENFILE);
                upper_fd = i;
                break;
            }
        }

        // fd = open("test_limited_open_fd", O_CREAT);
        // TASSERT(fd == -1, fd);
        /* TASSERT(errno == EMFILE, errno); */
        for (int i = 3; i < upper_fd; i++)
        {
            printf ("close %d\n", i);
            ret = sos_sys_close(i);
            TASSERT(ret == 0, ret);

            char tmp[80] = {0};
            sprintf(tmp, "test_limited_open_fd_%d", i);

            /* ret = sos_sys_remove(tmp); */
            /* TASSERT(ret == 0, 0); */
        }

        how_many_fd_generate_each_time[a] = fd_generate_counter;
    }

    for (int i=1 ; i < loop_times; i++)
    {
        // printf("i: %d, i-1: %d .\n", how_many_fd_generate_each_time[i -1] ,how_many_fd_generate_each_time[i]);
        TASSERT(how_many_fd_generate_each_time[i -1] == how_many_fd_generate_each_time[i], how_many_fd_generate_each_time[i]);
    }

    END_FUNCTION;
}

static void test_extreme_case_of_open_and_close(void)
{
    FUNCTION_CALL(test_invalid_sos_sys_close);
    FUNCTION_CALL(test_limited_open_fd);
    return;
}

static void test_permission(void)
{
    FUNCTION_CALL(test_readonly);
    FUNCTION_CALL(test_writeonly);
    FUNCTION_CALL(test_readandwrite);
    return;
}

static int test_write_multiple_consecutive_times(void)
{
    int fd = sos_sys_open("test_write", O_RDWR|O_CREAT);
    TASSERT(fd >= 0, fd);

    int ret = sos_sys_write(fd, "hello world", 11);
    TASSERT(ret == 11, ret);

    fd = update_internal_file_ptr(fd, "test_write", O_RDWR|O_CREAT);

    memset(buf, 0, BUF_SIZE);

    ret = sos_sys_read(fd, buf, 100);
    TASSERT(ret == 11, ret);
    TASSERT(strcmp(buf, "hello world") == 0, 0);

    fd = update_internal_file_ptr(fd, "test_write", O_RDWR|O_CREAT);

    int i = 0;
    int loop_times = 5;
    for(i = 0; i < loop_times; i++) {
        ret = sos_sys_write(fd, "hello world", 11);
        TASSERT(ret == 11, ret);
    }

    fd = update_internal_file_ptr(fd, "test_write", O_RDWR|O_CREAT);

    memset(buf, 0, BUF_SIZE);
    ret = sos_sys_read(fd, buf, 100);
    // 55 = 11 * 5
    TASSERT(ret == 55, ret);
    buf[ret] = 0;

    // printf("buf : %s \n", buf);

    for(i = 0; i < loop_times; i++) {
        char tmp[12];
        memcpy(tmp, buf+i*11, 11);
        tmp[11] = 0;
        ret = strcmp(tmp, "hello world");
        // printf("ret : %d tmp: %s \n", ret, tmp);
        TASSERT(ret == 0, 0);
    }

    ret = sos_sys_close(fd);
    TASSERT(ret == 0, 0);

    ret = sos_sys_remove("test_write");
    TASSERT(ret == 0, 0);
    return 0;
}

static int test_read_write_large_file(void)
{
    char tmp[BUF_SIZE] = {0};
    int i;
    for(i = 0; i < BUF_SIZE - 1; i++)
    {
        tmp[i] = (char) (i%26 + 65);
    }

    int fd = sos_sys_open("test_read_write_large_file", O_RDWR|O_CREAT);
    TASSERT(fd >= 0, fd);

    int ret = sos_sys_write(fd, tmp, BUF_SIZE - 1);
    TASSERT(ret == BUF_SIZE - 1, ret);

    fd = update_internal_file_ptr(fd, "test_read_write_large_file", O_RDWR|O_CREAT);

    memset(buf, 0, BUF_SIZE);
    ret = sos_sys_read(fd, buf, BUF_SIZE - 1);
    TASSERT(ret == BUF_SIZE - 1, ret);
    buf[ret] = 0;

    TASSERT(strcmp(tmp, buf) == 0, 0);

    sos_sys_close(fd);

    ret = sos_sys_remove("test_read_write_large_file");
    TASSERT( ret == 0, 0);
    return 0;
}

// static int test_invalid_read(void)
// {
//     BEGIN_FUNCTION;
//     int ret = read(19, buf, 5);
//     TASSERT(ret == -1, ret);
//     TASSERT(errno == EBADF, errno);

//     int fd = open ("test_invalid_read", O_CREAT|O_RDWR|O_TRUNC);
//     TASSERT(fd>=3, fd);

//     write(fd, "1", 1);
//     lseek(fd, 0, SEEK_SET);
//     char *p = (char *)0x131;
//     ret = read(fd, p, 5);
//     TASSERT(ret == -1, ret);
//     TASSERT(errno == EFAULT, errno);

//     printf ("sos_sys_close fd: %d\n" , fd);
//     sos_sys_close(fd);

//     ret = read(1, buf, 1);
//     TASSERT(ret == -1, ret);
//     TASSERT(errno == EBADF, errno);


//     END_FUNCTION;

// }
// static int test_invalid_write(void)
// {
//     BEGIN_FUNCTION;
//     int ret = write(19, "hello", 5);
//     TASSERT(ret == -1, ret);
//     TASSERT(errno == EBADF, errno);

//     int fd = open ("test_invalid_write", O_CREAT|O_RDWR|O_TRUNC);
//     TASSERT(fd>=3, fd);

//     ret = write(fd, 0x0, 5);
//     TASSERT(ret == -1, ret);
//     TASSERT(errno == EFAULT, errno);

//     sos_sys_close(fd);

//     ret = write(0, "h", 1);
//     TASSERT(ret == -1, ret);
//     TASSERT(errno == EBADF , errno);


//     END_FUNCTION;

// }
// static int test_sparse_file(void)
// {
//     BEGIN_FUNCTION;
//     int ret = 0;
//     int fd = open ("test_sparse_file", O_CREAT|O_RDWR|O_TRUNC);
//     TASSERT(fd>=3, fd);
//     TASSERT(lseek(fd, 10000, SEEK_SET) == 10000, 10000);

//     ret = write(fd, "sparse1", 7);
//     TASSERT(ret == 7, ret);
//     TASSERT(lseek(fd, 0, SEEK_SET) == 0, 0);

//     ret = read(fd, buf, sizeof(buf) - 1);
//     TASSERT(ret == (7 + 10000), ret);
//     TASSERT(buf[10001] == 'p', 'p');
//     sos_sys_close(fd);

//     END_FUNCTION;
// }

// static int test_iterative_write(void)
// {
//     BEGIN_FUNCTION;
//     int ret, fd;
//     fd = open("test_iterative_write", O_CREAT|O_RDWR|O_TRUNC);
//     for (int i = 0; i < 100; i ++)
//     {
//         ret = write(fd, "hello", 5);
//         TASSERT(ret == 5, ret);
//     }
//     lseek(fd, 0, SEEK_SET);
//     ret = read(fd, buf, 10000);
//     TASSERT(ret == 5 * 100, ret);
//     sos_sys_close(fd);

//     END_FUNCTION;
// }

// static void test_read_write(void)
// {
//     FUNCTION_CALL(test_invalid_write);
//     FUNCTION_CALL(test_write);
//     FUNCTION_CALL(test_sparse_file);
//     FUNCTION_CALL(test_iterative_write);

//     FUNCTION_CALL(test_invalid_read);
//     return;
// }

#define SMALL_BUF_SZ 2
#define BUF_SZ 5000

char test_str[] = "Basic test string for read/write";
char small_buf[SMALL_BUF_SZ];

int test_buffers(int console_fd) {
    /* test a small string from the code segment */
    int result = sos_sys_write(console_fd, test_str, strlen(test_str));
    assert(result == strlen(test_str));

    /* test reading to a small buffer */
    result = sos_sys_read(console_fd, small_buf, SMALL_BUF_SZ);
    /* make sure you type in at least SMALL_BUF_SZ */
    assert(result == SMALL_BUF_SZ);

    /* test reading into a large on-stack buffer */
    char stack_buf[BUF_SZ];
    /* for this test you'll need to paste a lot of data into
       the console, without newlines */

    printf ("%d\n", BUF_SZ);
    result = sos_sys_read(console_fd, stack_buf, BUF_SZ);
    printf ("%d %d\n", result, BUF_SZ);
    assert(result == BUF_SZ);

    result = sos_sys_write(console_fd, stack_buf, BUF_SZ);
    assert(result == BUF_SZ);

    /* this call to malloc should trigger an brk */
    char *heap_buf = malloc(BUF_SZ);
    assert(heap_buf != NULL);

    /* for this test you'll need to paste a lot of data into
       the console, without newlines */
    result = sos_sys_read(console_fd, heap_buf, BUF_SZ);
    assert(result == BUF_SZ);

    result = sos_sys_write(console_fd, heap_buf, BUF_SZ);
    assert(result == BUF_SZ);

    /* try sleeping */
    for (int i = 0; i < 5; i++) {
        time_t prev_seconds = time(NULL);
        sleep(1);
        time_t next_seconds = time(NULL);
        assert(next_seconds > prev_seconds);
        printf("Tick\n");
    }
    return 0;
}
void file_unittest(void)
{
    printf ("\n\n####### start file unitest ########\n");

    test_permission();
    test_extreme_case_of_open_and_close();
    FUNCTION_CALL(test_write_multiple_consecutive_times);
    FUNCTION_CALL(test_read_write_large_file);

    /*  */
    printf ("\n####### end file unitest ########\n\n");
    printf ("############## Statistic: Ran %d Test Case, %d Passed, %d Failed\n\n", total_count, pass_count, fail_count);
    return;
}

int console_test(void)
{
    close(0);
    int f = open("console", O_RDWR);
    test_buffers(f);
    close(f);
    assert(0==open("console", O_RDONLY));

    return 0;

}
