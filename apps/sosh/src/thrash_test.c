#include "thrash_test.h"
#include <stdlib.h>
#include <assert.h>

#define LARGE_SIZE 64*1024*1024*1024

static int large_STACK_arr[LARGE_SIZE] = {0};

void thrash_test()
{
	int large_DATA_arr[LARGE_SIZE];

	int * large_HEAP_arr = (int *)malloc(LARGE_SIZE * sizeof(int));

	for(int i = 0; i < LARGE_SIZE; i++) 
	{
		large_STACK_arr[i] = i;
		large_DATA_arr[i] = i;
		*(large_HEAP_arr + i) = i;
	}

	int test_loops = 10000;

	for(int i = 0; i < test_loops; i++)
    {    	
    	assert(large_STACK_arr[i] == i);
	    assert(large_DATA_arr[i] == i);
	    assert(*(large_STACK_arr + i) == i);
    }    
    
	int j,k;
	srand(sos_sys_time_stamp());
    for(int i = 0; i < test_loops; i++)
    {
    	j = rand();
    	k = rand();
    	
	    large_STACK_arr[j] = k;
	    assert(large_STACK_arr[j] == k);

	    large_DATA_arr[j] = k;
	    assert(large_DATA_arr[j] == k);

	    *(large_STACK_arr + j) = k;
	    assert(*(large_STACK_arr + j) == k);
    }    
    
    free(large_HEAP_arr);
}




