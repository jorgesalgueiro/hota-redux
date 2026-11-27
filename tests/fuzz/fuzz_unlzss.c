/*
 * libFuzzer target for the LZSS backdrop decoder.
 *
 * The fuzzer copies the input into the emulated VM memory and then calls
 * unlzss() with two offsets derived from the input. Build and run
 * instructions are in tests/fuzz/README.md.
 */
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "lzss.h"
#include "vm.h"

/* Normally defined by main.c; the fuzzer links only the decoder. */
int debug_flag = 0;

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
	unsigned char output[LZSS_OUTPUT_SIZE];
	unsigned char *memory = get_memory_ptr(0);
	size_t copy;
	int ptr1 = 0;
	int ptr2 = 0;

	if (memory == NULL)
	{
		return 0;
	}

	memset(memory, 0, VM_MEMORY_SIZE);
	copy = size < VM_MEMORY_SIZE ? size : VM_MEMORY_SIZE;
	if (copy > 0)
	{
		memcpy(memory, data, copy);
	}

	if (copy >= 4)
	{
		ptr1 = (((int)data[0] << 8) | data[1]) % (VM_MEMORY_SIZE - 8);
		ptr2 = (((int)data[2] << 8) | data[3]) % (VM_MEMORY_SIZE - 8);
	}

	unlzss(output, ptr1, ptr2);
	return 0;
}
