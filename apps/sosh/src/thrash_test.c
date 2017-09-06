#include "thrash_test.h"
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#define LARGE_SIZE (16*1024*1024 / 4)



static int large_DATA_arr[LARGE_SIZE];
static int queue[10000000][2] = {0};
static char hash[LARGE_SIZE] = {0};
int len = 0;

void thrash_test()
{
    len = 0;
    memset(hash, 0, sizeof(hash));
    int large_STACK_arr[LARGE_SIZE] ;

    /* int * large_HEAP_arr = (int *)malloc(LARGE_SIZE * sizeof(int)); */
    /* assert(large_HEAP_arr != NULL); */

    for(int i = 0; i < LARGE_SIZE; i++)
    {
        large_STACK_arr[i] = i;
        large_DATA_arr[i] = i;
        /* *(large_HEAP_arr + i) = i; */
    }

    int test_loops = LARGE_SIZE;

    for(int i = 0; i < test_loops; i++)
    {
        assert(large_STACK_arr[i] == i);
        assert(large_DATA_arr[i] == i);
    }

    int j,k;

    srand(sos_sys_time_stamp());
    for(int i = 0; i < test_loops; i++)
    {

        j = rand() % LARGE_SIZE;
        while (hash[j] == 1)
        {
            j = rand() % LARGE_SIZE;
        }
        hash[j] = 1;
        k = rand() ;
        queue[len][0] = j;
        queue[len][1] = k;
        len ++;

        large_STACK_arr[j] = k;
        assert(large_STACK_arr[j] == k);

        large_DATA_arr[j] = k;
        assert(large_DATA_arr[j] == k);

    }

    for (int i = 0; i < len ; ++ i)
    {
        assert(large_DATA_arr[queue[i][0]] == queue[i][1]);
        assert(large_STACK_arr[queue[i][0]] == queue[i][1]);
    }

}




