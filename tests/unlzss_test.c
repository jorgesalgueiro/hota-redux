/*
 * Regression test for the LZSS decoder: an all-zero bitstream used to
 * send the distance/match loops into an infinite search. The decoder must
 * reject it and return.
 */
#include <stdio.h>
#include <string.h>

#include "lzss.h"
#include "vm.h"

/* Normally provided by main.c; this test links only the decoder. */
int debug_flag = 0;

int main(void)
{
	unsigned char output[LZSS_OUTPUT_SIZE];
	unsigned char *memory = get_memory_ptr(0);

	if (memory == NULL)
	{
		fprintf(stderr, "no VM memory\n");
		return 1;
	}

	memset(memory, 0, VM_MEMORY_SIZE);
	if (unlzss(output, 0, 0) == 0)
	{
		fprintf(stderr, "zero stream was accepted\n");
		return 1;
	}

	printf("ok: zero stream rejected\n");
	return 0;
}
