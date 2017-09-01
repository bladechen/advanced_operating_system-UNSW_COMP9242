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
        printf("%d", (int)__LINE__);\
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
    ret = sos_sys_write(fd, "w1", 2);
    TASSERT(ret < -1, ret);
    TASSERT(sos_sys_close(fd) == 0, 0);
    return 0;
}

static int test_writeonly()
{
    int ret =  0;
    int fd = sos_sys_open("tp_tw", O_WRONLY| O_CREAT);
    TASSERT(fd >= 0, fd);
    ret = sos_sys_write(fd, "test_writeonly", 14);
    TASSERT(ret == 14, 14);
    ret = sos_sys_read(fd, buf, 14);
    TASSERT(ret < 0, ret);
    TASSERT(sos_sys_close(fd) == 0, 0);
    return 0;
}

static int test_readandwrite(void)
{
    int ret =  0;
    int fd = sos_sys_open("tp_raw", O_RDWR| O_CREAT);
    TASSERT(fd >= 0, fd);
    ret = sos_sys_write(fd, "test_readandwrite", 17);
    TASSERT(ret == 17, ret);
    ret = sos_sys_read(fd, buf, 17);
    TASSERT(ret == 17, ret);
    TASSERT(sos_sys_close(fd) == 0, 0);
    return 0;
}

// static int test_create_flag(void)
// {
//     char file_name[30] = {0};
//     int len = snprintf(file_name, 29, "test_create_flag_%s", random_str_generator());
//     file_name[len] = 0;
//     printf ("create file: %s\n", file_name);
//     int fd = open(file_name, 0);
//     TASSERT(fd == -1, fd);
//     TASSERT(errno == ENOENT, errno);
//     fd = open(file_name, O_CREAT);
//     TASSERT(fd > 0, fd);
//     TASSERT(sos_sys_close(fd) == 0, 0);
//     return 0;
// // }

// TODO: see what expect after send O_EXCL into it
// static int test_excl_flag(void)
// {
//     BEGIN_FUNCTION;
//     char file_name[30] = {0};
//     int len = snprintf(file_name, 29, "test_excl_flag_%s", random_str_generator());
//     file_name[len] = 0;
//     printf ("file: %s\n", file_name);
//     int fd = sos_sys_open(file_name, O_EXCL);
//     TASSERT(fd == -1, fd);

//     fd = open(file_name, O_EXCL|O_CREAT);
//     TASSERT(fd >= 3, fd);
//     sos_sys_close(fd);

//     fd = open(file_name, O_EXCL|O_CREAT);
//     TASSERT(fd == -1, fd);
//     TASSERT(errno  == EEXIST, errno);

//     fd = open(file_name, O_CREAT);
//     TASSERT(fd >=3 , fd);
//     sos_sys_close(fd);
//     END_FUNCTION;
// }

static int test_invalid_sos_sys_close(void)
{
    BEGIN_FUNCTION;
    int ret = sos_sys_close(339);
    TASSERT(ret < 0, ret);
    ret = sos_sys_close(-1);
    TASSERT(ret < 0, ret);
    END_FUNCTION;
}

// static int test_valid_sos_sys_close(void)
// {
//     BEGIN_FUNCTION;
//     int ret = 0;
//     ret = sos_sys_close(0);
//     TASSERT(ret == 0,ret);
//     ret = sos_sys_close(2);
//     TASSERT(ret == 0, ret);

//     ret = write(2, RED "should not seen!!\n" NONE, 20 + strlen(RED) + strlen(NONE));
//     TASSERT(ret < 0, ret);

//     int fd = open ("test_valid_sos_sys_close", O_CREAT);
//     ret = sos_sys_close(fd);


//     END_FUNCTION;
// }

static int test_limited_open_fd(void)
{
    BEGIN_FUNCTION;
    int ret = 0;
    /*
     * 128 is the upper limit
     */
    int fd = 0 ;
    int upper_fd = 0;
    for (int i = 0; i < MAX_FDNUM_PER_PROCESS; i ++)
    {
        char tmp[80] = {0};
        sprintf(tmp, "test_limited_open_fd_%d", i);
        fd = sos_sys_open(tmp, O_RDWR | O_CREAT);
        if (fd >= 0)
        {

            TASSERT(fd == i, i);
            upper_fd = i;
        }
        else
        {
            TASSERT(fd == -1, fd);
            // TASSERT(errno, ENFILE);
            // upper_fd = i;
            break;
        }
    }
    printf ("max fd num is: %d, the limited is %d\n", upper_fd, MAX_FDNUM_PER_PROCESS);

    // fd = open("test_limited_open_fd", O_CREAT);
    // TASSERT(fd == -1, fd);
    /* TASSERT(errno == EMFILE, errno); */
    for (int i = 0; i < upper_fd; i++)
    {
        ret = sos_sys_close(i);
        TASSERT(ret == 0, ret);
    }
    END_FUNCTION;
}


static void test_permission(void)
{
    FUNCTION_CALL(test_readonly);
    FUNCTION_CALL(test_writeonly);
    FUNCTION_CALL(test_readandwrite);
    return;
}
// static void test_other_open_flag()
// {
//     FUNCTION_CALL(test_create_flag);
//     FUNCTION_CALL(test_excl_flag);
//     return;
// }

// static void test_sos_sys_close()
// {
//     FUNCTION_CALL(test_iterative_sos_sys_close);
//     FUNCTION_CALL(test_invalid_sos_sys_close);
//     FUNCTION_CALL(test_valid_sos_sys_close);
//     return;
// }
// static void test_dup2()
// {
//     FUNCTION_CALL(test_invalid_dup2);
//     FUNCTION_CALL(test_dup2_to_stdio);
//     FUNCTION_CALL(test_dup2_to_file);
//     FUNCTION_CALL(test_iterative_dup2);
//     /* FUNCTION_CALL(test_dup2_to_file); */

//     return;
// }


// static void test_open(void)
// {
//     test_permission();
//     test_other_open_flag();
//     FUNCTION_CALL(test_limited_open_fd);
//     return;


// }

/*
*   No lseek function, so we have to close and open agian to 
*   refresh the pointer
*/
static int update_internal_file_ptr(int fd, const char * filename) {
    TASSERT(sos_sys_close(fd) == 0, 0);
    int fd_new = sos_sys_open(filename, O_RDWR|O_CREAT);
    TASSERT(fd_new >= 0, fd_new);
    return fd_new;
}

static int test_write_multiple_consecutive_times(void)
{
    int fd = sos_sys_open("test_write", O_RDWR|O_CREAT);
    TASSERT(fd >= 0, fd);

    int ret = sos_sys_write(fd, "hello world", 11);
    TASSERT(ret == 11, ret);

    fd = update_internal_file_ptr(fd, "test_write");

    memset(buf, 0, BUF_SIZE);

    ret = sos_sys_read(fd, buf, 100);
    TASSERT(ret == 11, ret);
    TASSERT(strcmp(buf, "hello world") == 0, 0);

    fd = update_internal_file_ptr(fd, "test_write");

    int i = 0; 
    int loop_times = 5;
    for(i = 0; i < loop_times; i++) {
        ret = sos_sys_write(fd, "hello world", 11);
        TASSERT(ret == 11, ret);    
    }
    
    fd = update_internal_file_ptr(fd, "test_write");

    memset(buf, 0, BUF_SIZE);
    ret = sos_sys_read(fd, buf, 100);
    // 55 = 11 * 5
    TASSERT(ret == 55, ret);
    buf[ret] = 0;

    for(i = 0; i < loop_times; i++) {
        char tmp[11];
        memcpy(tmp, buf+i*11, 11);
        ret = strcmp(tmp, "hello world" );
        TASSERT( ret == 0, 0);
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

    fd = update_internal_file_ptr(fd, "test_read_write_large_file");

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
static int test_sparse_file(void)
{
    BEGIN_FUNCTION;
    int ret = 0;
    int fd = open ("test_sparse_file", O_CREAT|O_RDWR|O_TRUNC);
    TASSERT(fd>=3, fd);
    TASSERT(lseek(fd, 10000, SEEK_SET) == 10000, 10000);

    ret = write(fd, "sparse1", 7);
    TASSERT(ret == 7, ret);
    TASSERT(lseek(fd, 0, SEEK_SET) == 0, 0);

    ret = read(fd, buf, sizeof(buf) - 1);
    TASSERT(ret == (7 + 10000), ret);
    TASSERT(buf[10001] == 'p', 'p');
    sos_sys_close(fd);

    END_FUNCTION;
}

static int test_iterative_write(void)
{
    BEGIN_FUNCTION;
    int ret, fd;
    fd = open("test_iterative_write", O_CREAT|O_RDWR|O_TRUNC);
    for (int i = 0; i < 100; i ++)
    {
        ret = write(fd, "hello", 5);
        TASSERT(ret == 5, ret);
    }
    lseek(fd, 0, SEEK_SET);
    ret = read(fd, buf, 10000);
    TASSERT(ret == 5 * 100, ret);
    sos_sys_close(fd);

    END_FUNCTION;
}

static void test_read_write(void)
{
    FUNCTION_CALL(test_invalid_write);
    FUNCTION_CALL(test_write);
    FUNCTION_CALL(test_sparse_file);
    FUNCTION_CALL(test_iterative_write);

    FUNCTION_CALL(test_invalid_read);
    return;
}

void file_unittest(void)
{
    printf ("\n\n####### start file unitest ########\n");
    // printf ("%s\n", random_str_generator());

    // test_dup2();
    // test_open();
    //  test_open(); 

    // test_lseek();
    // test_read_write();

    // test_sos_sys_close();
    FUNCTION_CALL(test_write_multiple_consecutive_times);
    FUNCTION_CALL(test_read_write_large_file);


    printf ("\n####### end file unitest ########\n\n");
    printf ("############## Statistic: Ran %d Test Case, %d Passed, %d Failed\n\n", total_count, pass_count, fail_count);
    return;
}
