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

#define MAX_FDNUM_PER_PROCESS 128
#define BUF_SIZE              100000

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

void file_unittest(void);
static int pass_count = 0;
static int total_count = 0;

static int fail_count = 0;

static char buf[BUF_SIZE] = {0};
static int buf_len  = 0;

// #define TASSERT(expr, v) do {\
//     if (!(expr)) \
//     {\
//         printf( CYAN "\n################ TASSERT ############\n ");\
//         write(1, __FILE__, sizeof(__FILE__));\
//         write(1, "(",1 );\
//         printf("%d", (int)__LINE__);\
//         write(1, "):",2 );\
//         write(1, __func__, sizeof(__func__));\
//         write(1, "--------->", 10);\
//         write(1, #expr, sizeof(#expr)); \
//         printf(", but value is %d  ", (int)v);\
//         write(1, "\n--->", 5);\
//         printf("errno : %d, msg: %s\n" NONE , errno, strerror(errno));\
//         return -1;\
//     }\
// }while(0)


// #define FUNCTION_CALL(func, args...) do{\
//     int tmp = 0;\
//     printf (YELLOW "\nrunning: %s\n" NONE, #func);\
//     tmp = func(args);\
//     (tmp == 0) ? (pass_count ++ ): (fail_count ++);\
//     total_count ++;\
//     if (tmp == 0) {\
//         printf (GREEN "%s passed\n" NONE, #func);\
//     } else {\
//         printf (RED "%s failed\n" NONE, #func);\
//     }\
// } while(0)

// #define BEGIN_FUNCTION {}
// #define END_FUNCTION return 0


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
static int test_readonly(int a, int b)
{
    int ret =  0;
    a = 3;
    b = 5;
    TASSERT(a !=b , 0);
    int fd = open("tp_tr", O_RDONLY| O_CREAT);
    TASSERT(fd >= 0, fd);
    ret = read(fd, buf, 100);
    TASSERT(ret >= 0, ret);
    ret = write(fd, "w1", 2);
    TASSERT(ret == -1, ret);
    TASSERT(close(fd) == 0, 0);
    return 0;
}
static int test_writeonly()
{
    int ret =  0;
    int fd = open("tp_tw", O_WRONLY| O_CREAT);
    TASSERT(fd >= 0, fd);
    ret = read(fd, buf, 100);
    TASSERT(ret == -1, ret);
    ret = write(fd, "w1", 2);
    TASSERT(ret == 2, 2);
    TASSERT(close(fd) == 0, 0);
    return 0;
}
static int test_readandwrite(void)
{
    int ret =  0;
    int fd = open("tp_raw", O_RDWR| O_CREAT);
    ret = write(fd, "w1", 2);
    TASSERT(ret == 2, ret);
    TASSERT(fd >= 0, fd);
    ret = read(fd, buf, 100);
    TASSERT(ret >= 0, ret);
    TASSERT(close(fd) == 0, 0);
    return 0;
}

static int test_create_flag(void)
{
    char file_name[30] = {0};
    int len = snprintf(file_name, 29, "test_create_flag_%s", random_str_generator());
    file_name[len] = 0;
    printf ("create file: %s\n", file_name);
    int fd = open(file_name, 0);
    TASSERT(fd == -1, fd);
    TASSERT(errno == ENOENT, errno);
    fd = open(file_name, O_CREAT);
    TASSERT(fd >= 3, fd);
    TASSERT(close(fd) == 0, 0);
    return 0;
}
static int test_excl_flag(void)
{
    BEGIN_FUNCTION;
    char file_name[30] = {0};
    int len = snprintf(file_name, 29, "test_excl_flag_%s", random_str_generator());
    file_name[len] = 0;
    printf ("file: %s\n", file_name);
    int fd = open(file_name, O_EXCL);
    TASSERT(fd == -1, fd);

    fd = open(file_name, O_EXCL|O_CREAT);
    TASSERT(fd >= 3, fd);
    close(fd);

    fd = open(file_name, O_EXCL|O_CREAT);
    TASSERT(fd == -1, fd);
    TASSERT(errno  == EEXIST, errno);

    fd = open(file_name, O_CREAT);
    TASSERT(fd >=3 , fd);
    close(fd);
    END_FUNCTION;
}
static int test_invalid_close(void)
{
    BEGIN_FUNCTION;
    int ret = close(339);
    TASSERT(errno == EBADF, errno);
    TASSERT(ret == -1, ret);

    ret = close(-1);
    TASSERT(errno == EBADF, errno);
    TASSERT(ret == -1, ret);

    END_FUNCTION;
}
static int test_invalid_dup2(void)
{
    BEGIN_FUNCTION;
    int ret = 0;
    /* int fd = open("test_invalid_dup2", O_CREAT|O_RDWR); */
    ret = dup2(100, 101);
    TASSERT(ret == -1, ret);
    TASSERT(errno == EBADF, errno);

    ret = dup2(100, 1);
    TASSERT(ret == -1, ret);
    TASSERT(errno == EBADF, errno);

    /* ret = dup2(1, 100); */
    /* TASSERT(ret == -1, ret); */
    /* TASSERT(errno == EBADF, errno); */


    END_FUNCTION;
}
static int test_dup2_to_file(void)
{
    char buf[20];
    int buf_len = 20;
    BEGIN_FUNCTION;
    int fd = open("test_dup2_to_file", O_CREAT|O_RDWR|O_TRUNC);
    TASSERT(fd >= 0, fd);
    int ret = dup2(2, 100);
    TASSERT(ret >= 0, ret);
    ret = dup2(fd, 2);
    TASSERT(ret >= 0, ret);

    ret = write(2, "hello", 5);
    TASSERT(ret == 5, ret);

    lseek(2, 0, SEEK_SET);
    ret = read(2, buf, buf_len);
    TASSERT(ret == 5, ret);

    lseek(fd, 0, SEEK_SET);
    ret = read(fd, buf, buf_len);
    TASSERT(ret == 5, ret);

    close(2);

    lseek(fd, 0, SEEK_SET);
    ret = read(fd, buf, buf_len);
    TASSERT(ret == 5, ret);

    lseek(2, 0, SEEK_SET);
    ret = read(2, buf, buf_len);
    TASSERT(ret == -1, ret);


    ret = dup2(100, 2);
    TASSERT(ret >= 0, ret);
    ret = close(100);
    TASSERT(ret>= 0, ret);

    ret = close(fd);
    TASSERT(ret >= 0, ret);
    END_FUNCTION;
}
static int test_dup2_to_stdio(void)
{
    BEGIN_FUNCTION;

    int ret = dup2(2, 100);
    TASSERT(ret == 0, ret);

    ret = write(100, RED "should be seen, 1!!\n" NONE, 20 + strlen(RED) + strlen(NONE));
    TASSERT(ret > 0 , ret);
    ret = close(100);
    TASSERT(ret == 0, ret);

    ret = write(2, RED "should be seen, 2!!\n" NONE, 20 + strlen(RED) + strlen(NONE));
    TASSERT(ret > 0 , ret);

    ret = write(100, RED "should not be seen, 1!!\n" NONE, 24 + strlen(RED) + strlen(NONE));
    TASSERT(ret == -1, ret);
    TASSERT(errno == EBADF, errno);


    int fd = open ("test_dup2_to_stdio", O_CREAT|O_RDWR);
    TASSERT(fd >= 3, fd);
    ret = write(fd, "hello\n", 6);
    TASSERT(ret == 6, ret);
    ret = dup2(2, fd);
    ret = write(fd, RED "should be seen, 3!!\n" NONE, 20 + strlen(RED) + strlen(NONE));
    TASSERT(ret > 0, ret );

    close(fd);
    ret = write(fd, RED "should not be seen, 2!!\n" NONE, 24 + strlen(RED) + strlen(NONE));
    TASSERT(ret == -1, ret);
    TASSERT(errno == EBADF, errno);

    ret = write(2, RED "should be seen, 4!!\n" NONE, 20 + strlen(RED) + strlen(NONE));
    TASSERT(ret > 0, ret);

    END_FUNCTION;
}
static int test_valid_close(void)
{
    BEGIN_FUNCTION;
    int ret = 0;
    ret = close(0);
    TASSERT(ret == 0,ret);
    ret = close(2);
    TASSERT(ret == 0, ret);

    ret = write(2, RED "should not seen!!\n" NONE, 20 + strlen(RED) + strlen(NONE));
    TASSERT(ret < 0, ret);

    int fd = open ("test_valid_close", O_CREAT);
    ret = close(fd);


    END_FUNCTION;
}
static int test_limited_open_fd(void)
{
    BEGIN_FUNCTION;
    int ret = 0;
    /*
     * 128 is the upper limit, 0, 1, 2 is for stdio, so there are still 125 fd could be used in
     * one process
     * but the lower fs may support less than 128
     */
    int fd = 0 ;
    int upper_fd = 0;
    for (int i = 3; i < MAX_FDNUM_PER_PROCESS + 1; i ++)
    {
        fd = open("test_limited_open_fd", O_CREAT);
        if (fd != -1)
        {

            TASSERT(fd == i, i);
            upper_fd = i;
        }
        else
        {
            TASSERT(fd == -1, fd);
            TASSERT(errno, ENFILE);
            upper_fd = i;
            break;
        }
    }
    printf ("max fd num is: %d, the limited is %d\n", upper_fd, MAX_FDNUM_PER_PROCESS);

    fd = open("test_limited_open_fd", O_CREAT);
    TASSERT(fd == -1, fd);
    /* TASSERT(errno == EMFILE, errno); */

    for (int i = 3; i < upper_fd; i ++)
    {
        ret = close(i);
        TASSERT(ret == 0, ret);
    }
    END_FUNCTION;
}

static int test_iterative_close(void)
{
    BEGIN_FUNCTION;
    for (int i = 3; i < MAX_FDNUM_PER_PROCESS; i ++)
    {
        printf ("close fd: %d\n" ,i);
        int ret = close(i);
        TASSERT(ret == -1, ret);
        TASSERT(errno == EBADF, errno);
    }
    END_FUNCTION;
}
static int test_iterative_dup2(void)
{
    BEGIN_FUNCTION;
    int ret = 0;
    for (int i = 3; i < MAX_FDNUM_PER_PROCESS; i ++)
    {
        ret = dup2(1, i);
        TASSERT(ret == 0, ret);
    }
    ret = dup2( 1, MAX_FDNUM_PER_PROCESS );
    TASSERT(ret == -1, ret);

    for (int i = 3; i < MAX_FDNUM_PER_PROCESS; i ++)
    {
        ret = close(i);
        TASSERT(ret == 0, ret);
    }

    END_FUNCTION;

}
static void test_permission(void)
{
    FUNCTION_CALL(test_readonly, 1, 2);
    FUNCTION_CALL(test_writeonly);
    FUNCTION_CALL(test_readandwrite);
    return;
}
static void test_other_open_flag()
{
    FUNCTION_CALL(test_create_flag);
    FUNCTION_CALL(test_excl_flag);
    return;
}

static void test_close()
{
    FUNCTION_CALL(test_iterative_close);
    FUNCTION_CALL(test_invalid_close);
    FUNCTION_CALL(test_valid_close);
    return;
}
static void test_dup2()
{
    FUNCTION_CALL(test_invalid_dup2);
    FUNCTION_CALL(test_dup2_to_stdio);
    FUNCTION_CALL(test_dup2_to_file);
    FUNCTION_CALL(test_iterative_dup2);
    /* FUNCTION_CALL(test_dup2_to_file); */

    return;
}


static void test_open(void)
{
    test_permission();
    test_other_open_flag();
    FUNCTION_CALL(test_limited_open_fd);
    return;


}


static int test_invalid_lseek(void)
{
    BEGIN_FUNCTION;
    int ret = 0;
    ret = lseek(1, 100, SEEK_SET);
    TASSERT(ret == -1, ret);
    TASSERT(errno == ESPIPE, errno);

    ret = lseek(0, 12, SEEK_END);
    TASSERT(ret == -1, ret);
    TASSERT(errno == ESPIPE, errno);

    ret = lseek(2, 121, SEEK_CUR);
    TASSERT(ret == -1, ret);
    TASSERT(errno == ESPIPE, errno);


    ret = lseek(3, 12, SEEK_SET);
    TASSERT(ret == -1, ret);
    TASSERT(errno == EBADF, errno);

    int fd = open("test_invalid_lseek", O_CREAT|O_RDWR|O_TRUNC);
    TASSERT(fd == 3, fd);

    ret = lseek(3, 101, 3);
    TASSERT(ret == -1, ret);
    TASSERT(errno == EINVAL, errno);

    ret = lseek(3, 1213, -1);
    TASSERT(ret == -1, ret);
    TASSERT(errno == EINVAL, errno);

    ret = lseek(3, -123432, SEEK_SET);
    TASSERT(ret == -1, ret);
    TASSERT(errno == EINVAL, errno);

    ret = lseek(3, 10, SEEK_SET);
    TASSERT(ret == 10, ret);

    ret = lseek(3, -11, SEEK_CUR);
    TASSERT(ret == -1, ret);
    TASSERT(errno == EINVAL, errno);

    ret = lseek(3, 0, SEEK_SET);
    ret = write(3, "hello", 5);
    TASSERT(ret == 5, ret);

    ret = lseek(3, -6, SEEK_END);
    TASSERT(ret == -1, ret);
    TASSERT(errno == EINVAL, errno);


    close(3);

    END_FUNCTION;
}

int test_valid_lseek(void);
int test_valid_lseek(void)
{
    BEGIN_FUNCTION;
    off_t ret = 0;
    int fd = open("test_valid_lseek", O_CREAT|O_TRUNC|O_RDWR);
    TASSERT(fd == 3, fd);

    ret = lseek(fd, 30000000000LL, SEEK_SET);
    /* printf ("lseek pos: %lld\n", ret); */
    TASSERT(ret == 30000000000LL, ret);

    ret = lseek(fd, 20000000000LL, SEEK_CUR);
    TASSERT(ret == 50000000000LL, ret);

    ret = lseek(fd, -40000000000LL, SEEK_CUR);
    TASSERT(ret == 10000000000LL, ret);

    ret = lseek(fd, -40000000000LL, SEEK_CUR);
    TASSERT(ret == -1, ret);
    TASSERT(errno == EINVAL, errno);


    ret = lseek(fd, -10000000000LL, SEEK_CUR);
    TASSERT(ret == 0, ret);

    ret = lseek(fd,  5, SEEK_END);
    TASSERT(ret == 5, ret);

    ret = lseek(fd, 10000000000LL, SEEK_SET);
    TASSERT(ret == 10000000000LL, ret);

    ret = lseek(fd, 0, SEEK_CUR);
    TASSERT(ret == 10000000000LL, ret);

    close(3);
    END_FUNCTION;
}

static void test_lseek(void)
{

    FUNCTION_CALL(test_invalid_lseek);
    FUNCTION_CALL(test_valid_lseek);
    return;

}

static int test_write(void)
{
    (void) buf_len;
    int fd = open("test_write", O_RDWR|O_CREAT|O_TRUNC);
    TASSERT(fd >= 3, fd);

    int ret = write(fd, "hello world", 11);
    TASSERT(ret == 11, ret);

    TASSERT(lseek(fd, 0, SEEK_SET) == 0, 0);
    ret = read(fd, buf, 100);
    TASSERT(ret == 11, ret);
    buf[ret] = 0;
    TASSERT(strcmp(buf, "hello world" ) == 0, 0);


    TASSERT(lseek(fd, 1000, SEEK_SET) == 1000, 0);

    ret = write(fd, "hello world", 11);
    TASSERT(ret == 11, ret);
    TASSERT(lseek(fd, 1000, SEEK_SET) == 1000, 0);
    ret = read(fd, buf, 100);
    TASSERT(ret == 11, ret);
    buf[ret] = 0;
    TASSERT(strcmp(buf, "hello world" ) == 0, 0);

    close(fd);

    return 0;
}
static int test_invalid_read(void)
{
    BEGIN_FUNCTION;
    int ret = read(19, buf, 5);
    TASSERT(ret == -1, ret);
    TASSERT(errno == EBADF, errno);

    int fd = open ("test_invalid_read", O_CREAT|O_RDWR|O_TRUNC);
    TASSERT(fd>=3, fd);

    write(fd, "1", 1);
    lseek(fd, 0, SEEK_SET);
    char *p = (char *)0x131;
    ret = read(fd, p, 5);
    TASSERT(ret == -1, ret);
    TASSERT(errno == EFAULT, errno);

    printf ("close fd: %d\n" , fd);
    close(fd);

    ret = read(1, buf, 1);
    TASSERT(ret == -1, ret);
    TASSERT(errno == EBADF, errno);


    END_FUNCTION;

}
static int test_invalid_write(void)
{
    BEGIN_FUNCTION;
    int ret = write(19, "hello", 5);
    TASSERT(ret == -1, ret);
    TASSERT(errno == EBADF, errno);

    int fd = open ("test_invalid_write", O_CREAT|O_RDWR|O_TRUNC);
    TASSERT(fd>=3, fd);

    ret = write(fd, 0x0, 5);
    TASSERT(ret == -1, ret);
    TASSERT(errno == EFAULT, errno);

    close(fd);

    ret = write(0, "h", 1);
    TASSERT(ret == -1, ret);
    TASSERT(errno == EBADF , errno);


    END_FUNCTION;

}
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
    close(fd);

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
    close(fd);

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
    printf ("%s\n", random_str_generator());

    test_dup2();
    test_open();
    /* test_open(); */

    test_lseek();
    test_read_write();

    test_close();


    printf ("\n####### end file unitest ########\n\n");
    printf ("############## Statistic: Ran %d Test Case, %d Passed, %d Failed\n\n", total_count, pass_count, fail_count);
    return;
}
