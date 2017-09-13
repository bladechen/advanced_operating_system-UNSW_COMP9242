
/*
 * Copyright 2014, NICTA
 *
 * This software may be distributed and modified according to the terms of
 * the BSD 2-Clause license. Note that NO WARRANTY is provided.
 * See "LICENSE_BSD2.txt" for details.
 *
 * @TAG(NICTA_BSD)
 */

/* Simple shell to run on SOS */

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

/* Your OS header file */
#include <sos.h>
#include "file-system-unit-test.h"
#include "thrash_test.h"
#include "benchmark.h"

#define BUF_SIZ    6144
#define MAX_ARGS   32

static int in;
static sos_stat_t sbuf;

static void prstat(const char *name) {
    /* print out stat buf */
    /* printf ("hello %c %c %c %c\n",   sbuf.st_type == ST_SPECIAL ? 's' : '-' , sbuf.st_fmode & FM_READ ? 'r' : '-', */
            /* sbuf.st_fmode & FM_WRITE ? 'w' : '-', sbuf.st_fmode & FM_EXEC ? 'x' : '-'); */
    printf("%c%c%c%c 0x%06x 0x%06lx 0x%06lx %s\n",
           sbuf.st_type == ST_SPECIAL ? 's' : '-',
           sbuf.st_fmode & FM_READ ? 'r' : '-',
           sbuf.st_fmode & FM_WRITE ? 'w' : '-',
           sbuf.st_fmode & FM_EXEC ? 'x' : '-', sbuf.st_size, sbuf.st_ctime,
           sbuf.st_atime, name);
    /* printf ("world\n"); */
}

static int cat(int argc, char **argv) {
    int fd;
    char buf[BUF_SIZ];
    int num_read, stdout_fd, num_written = 0;


    if (argc != 2) {
        printf("Usage: cat filename\n");
        return 1;
    }

    printf("<%s>\n", argv[1]);

    fd = open(argv[1], O_RDONLY);
    stdout_fd = open("console", O_WRONLY);
    if (fd < 0)
    {
        close(fd);
        close(stdout_fd);
        printf ("error\n");
        return 2;
    }

    assert(fd >= 0);

    while ((num_read = read(fd, buf, BUF_SIZ)) > 0)
        num_written = write(stdout_fd, buf, num_read);

    close(stdout_fd);
    close(fd);
    close(stdout_fd);


    if (num_read == -1 || num_written == -1) {
        printf("error on write\n");
        return 1;
    }

    return 0;
}

static int cp(int argc, char **argv) {
    int fd, fd_out;
    char *file1, *file2;
    char buf[BUF_SIZ];
    int num_read, num_written = 0;

    if (argc != 3) {
        printf("Usage: cp from to\n");
        return 1;
    }

    file1 = argv[1];
    file2 = argv[2];

    fd = open(file1, O_RDONLY);
    fd_out = open(file2, O_WRONLY);

    if (fd < 0 || fd_out < 0)
    {
        printf ("something error\n");
        close(fd);
        close(fd_out);
        return 2;
    }
    assert(fd >= 0);
    assert(fd_out >= 0);

    while ((num_read = read(fd, buf, BUF_SIZ)) > 0)
        num_written = write(fd_out, buf, num_read);

    if (num_read == -1 || num_written == -1) {
        close(fd);
        close(fd_out);


        printf("error on cp\n");
        return 1;
    }
    close(fd);
    close(fd_out);


    return 0;
}

#define MAX_PROCESSES 10

static int ps(int argc, char **argv) {
    sos_process_t *process;
    int i, processes;

    process = malloc(MAX_PROCESSES * sizeof(*process));

    if (process == NULL) {
        printf("%s: out of memory\n", argv[0]);
        return 1;
    }

    processes = sos_process_status(process, MAX_PROCESSES);

    printf("TID SIZE   STIME   CTIME COMMAND\n");

    for (i = 0; i < processes; i++) {
        printf("%3d %4d %7d %s\n", process[i].pid, process[i].size,
               process[i].stime, process[i].command);
    }

    free(process);

    return 0;
}

static int exec(int argc, char **argv) {
    pid_t pid;
    int r;
    int bg = 0;

    if (argc < 2 || (argc > 2 && argv[2][0] != '&')) {
        printf("Usage: exec filename [&]\n");
        return 1;
    }

    if ((argc > 2) && (argv[2][0] == '&')) {
        bg = 1;
    }

    if (bg == 0) {
        r = close(in);
        assert(r == 0);
    }

    pid = sos_process_create(argv[1]);
    if (pid >= 0) {
        printf("Child pid=%d\n", pid);
        if (bg == 0) {
            sos_process_wait(pid);
        }
    } else {
        printf("Failed!\n");
    }
    if (bg == 0) {
        in = open("console", O_RDONLY);
        assert(in >= 0);
    }
    return 0;
}

static int dir(int argc, char **argv) {
    int i = 0, r;
    char buf[BUF_SIZ];

    if (argc > 2) {
        printf("usage: %s [file]\n", argv[0]);
        return 1;
    }

    if (argc == 2) {
        r = sos_stat(argv[1], &sbuf);
        if (r < 0) {
            printf("stat(%s) failed: %d\n", argv[1], r);
            return 0;
        }
        prstat(argv[1]);
        return 0;
    }

    while (1) {
        tty_debug_print("start ls loop: %d\n",i);
        r = sos_getdirent(i, buf, BUF_SIZ);
        if (r < 0) {
            printf("dirent(%d) failed: %d\n", i, r);
            break;
        } else if (!r) {
            break;
        }
        r = sos_stat(buf, &sbuf);
        if (r < 0) {
            printf("stat(%s) failed: %d\n", buf, r);
            break;
        }
        tty_debug_print("finish ls loop: %s %d\n", buf, i);
        prstat(buf);
        i++;
    }
    return 0;
}

static int test_file_syscall(int argc, char **argv) {
    (void) argc;
    (void)argv;
    file_unittest();
    return 0;
}

static int rm(int argc, char **argv) {
    int i = 0, r;

    if (argc > 2) {
        printf("usage: %s [file]\n", argv[0]);
        return 1;
    }

    if (argc == 2) {
        r = sos_sys_remove(argv[1]);
        if (r < 0) {
            printf("stat(%s) failed: %d\n", argv[1], r);
        }
        return 0;
    }

    return 0;
}


static int second_sleep(int argc,char *argv[]) {
    if (argc != 2) {
        printf("Usage %s seconds\n", argv[0]);
        return 1;
    }
    sleep(atoi(argv[1]));
    return 0;
}

static int milli_sleep(int argc,char *argv[]) {
    struct timespec tv;
    uint64_t nanos;
    if (argc != 2) {
        printf("Usage %s milliseconds\n", argv[0]);
        return 1;
    }
    nanos = (uint64_t)atoi(argv[1]) * NS_IN_MS;
    /* Get whole seconds */
    tv.tv_sec = nanos / NS_IN_S;
    /* Get nanos remaining */
    tv.tv_nsec = nanos % NS_IN_S;
    nanosleep(&tv, NULL);
    return 0;
}

static int second_time(int argc, char *argv[]) {
    printf("%d seconds since boot\n", (int)time(NULL));
    return 0;
}

static int micro_time(int argc, char *argv[]) {
    struct timeval time;
    gettimeofday(&time, NULL);
    uint64_t micros = (uint64_t)time.tv_sec * US_IN_S + (uint64_t)time.tv_usec;
    printf("%llu microseconds since boot\n", micros);
    return 0;
}

static int kill(int argc, char *argv[]) {
    pid_t pid;
    if (argc != 2) {
        printf("Usage: kill pid\n");
        return 1;
    }

    pid = atoi(argv[1]);
    return sos_process_delete(pid);
}

static int benchmark(int argc, char *argv[]) {
    if(argc == 2 && strcmp(argv[1], "-d") == 0) {
        printf("Running benchmark in DEBUG mode\n");
        return sos_benchmark(1);
    } else if (argc == 1) {
        printf("Running benchmark\n");
        return sos_benchmark(0);
    } else {
        printf("Unknown option to %s\n", argv[0]);
        return -1;
    }
}

static int die()
{
    *(int*)(0) =  1313;
}

static int thrash()
{
    thrash_test();
}

struct command {
    char *name;
    int (*command)(int argc, char **argv);
};

struct command commands[] = { { "dir", dir }, { "ls", dir }, { "cat", cat }, {
    "cp", cp }, { "ps", ps }, { "exec", exec }, {"sleep",second_sleep}, {"msleep",milli_sleep},
               {"time", second_time}, {"mtime", micro_time}, {"kill", kill},
               {"benchmark", benchmark}, {"rm", rm}, {"test_file_syscall", test_file_syscall},
               {"thrash", thrash}, {"die", die}};

void simple_file_test()
{
    char buf[BUF_SIZ];
    char *argv[MAX_ARGS];
    int i, r, done, found, new, argc;
    char *bp, *p;

    // r =  sos_getdirent(1, buf, 1000);
    // assert(r > 0);
    // buf[r] = 0;
    // tty_debug_print("get file : %s\n", buf);

    // r =  sos_getdirent(3, buf, 1000);
    // assert(r > 0);
    // buf[r] = 0;
    // tty_debug_print("get file : %s\n", buf);

    // r =  sos_getdirent(10, buf, 1000);
    // assert(r < 0);

    // r =  sos_getdirent(4, buf, 1000);
    // assert(r == 0);
    // /* while(1){} */
    // sos_stat_t stat;
    // assert(0 == sos_stat("hello_world", &stat));
    // // TODO check status
    // tty_debug_print("%u %u %u %llu %llu\n", stat.st_size, stat.st_fmode, stat.st_type, stat.st_ctime, stat.st_atime);
    /* assert( open("hello_world", O_RDONLY ) < 0); */
    in = open("hello_world",  O_RDWR| O_CREAT  );

    for(int i=0; i<BUF_SIZ; i++) {
        buf[i] = 'a';
    }

    int ret = write(in, buf, BUF_SIZ);
    // assert(ret == BUF_SIZ);
    tty_debug_print("sosh write to `hello_world` ret: %d\n",  ret);

    memset(buf, 0, BUF_SIZ);
    // assert( open("hello_world", O_RDONLY ) > 0);
    ret = read(in, buf, BUF_SIZ - 1);
    // tty_debug_print("read return value: %d\n", ret);
    // assert(ret == BUF_SIZ - 1);
    // buf[ret] = 0;
    tty_debug_print("sosh read len %d\n",  ret);
    // ret = write(in, buf, BUF_SIZ - 1);
    // assert(ret == BUF_SIZ - 1);
    tty_debug_print("sosh read from `hello_world`: %s\n",  buf);

    close(in);
    // close(in + 1);

    sos_sys_remove("hello_world");
    // close(in + 1);
}

int main(void) {
    char buf[BUF_SIZ];
    char *argv[MAX_ARGS];
    int i, r, done, found, new, argc;
    char *bp, *p;
    in = open("console", O_RDONLY);
    assert(in >= 0);
    bp = buf;
    done = 0;
    new = 1;

    /* stress tests for file operations */
    /* file_unittest(); */

    /* while (1){} */
    printf("\n[SOS SHELL Starting]\n");

    while (!done) {
        if (new) {
            printf("$ ");
        }
        new = 0;
        found = 0;

        while (!found && !done) {
            /* Make sure to flush so anything is visible while waiting for user input */
            fflush(stdout);
            r = read(in, bp, BUF_SIZ - 1 + buf - bp);
            if (r < 0) {
                printf("Console read failed!\n");
                done = 1;
                break;
            }
            bp[r] = 0; /* terminate */

            tty_debug_print("[sosh] read user input: %s\n", bp);
            for (p = bp; p < bp + r; p++) {
                if (*p == '\03') { /* ^C */
                    printf("^C\n");
                    p = buf;
                    new = 1;
                    break;
                } else if (*p == '\04') { /* ^D */
                    p++;
                    found = 1;
                } else if (*p == '\010' || *p == 127) {
                    /* ^H and BS and DEL */
                    if (p > buf) {
                        printf("\010 \010");
                        p--;
                        r--;
                    }
                    p--;
                    r--;
                } else if (*p == '\n') { /* ^J */
                    printf("%c", *p);
                    *p = 0;
                    found = p > buf;
                    p = buf;
                    new = 1;
                    break;
                } else {
                    printf("%c", *p);
                }
            }
            bp = p;
            if (bp == buf) {
                break;
            }
        }

        if (!found) {
            continue;
        }

        argc = 0;
        p = buf;

        while (*p != '\0') {
            /* Remove any leading spaces */
            while (*p == ' ')
                p++;
            if (*p == '\0')
                break;
            argv[argc++] = p; /* Start of the arg */
            while (*p != ' ' && *p != '\0') {
                p++;
            }

            if (*p == '\0')
                break;

            /* Null out first space */
            *p = '\0';
            p++;
        }

        if (argc == 0) {
            continue;
        }

        found = 0;
        tty_debug_print("[sosh] executing command, argc: %d, %s\n", argc, argv[0]);

        for (i = 0; i < sizeof(commands) / sizeof(struct command); i++) {
            if (strcmp(argv[0], commands[i].name) == 0) {
                commands[i].command(argc, argv);
                found = 1;
                break;
            }
        }

        /* Didn't find a command */
        if (found == 0) {
            /* They might try to exec a program */
            if (sos_stat(argv[0], &sbuf) != 0) {
                printf("Command \"%s\" not found\n", argv[0]);
            } else if (!(sbuf.st_fmode & FM_EXEC)) {
                printf("File \"%s\" not executable\n", argv[0]);
            } else {
                /* Execute the program */
                argc = 2;
                argv[1] = argv[0];
                argv[0] = "exec";
                exec(argc, argv);
            }
        }
    }
    printf("[SOS Exiting]\n");
}
