/*
 * Heart of The Alien: Virtual machine primitives (memory and variables)
 * Copyright (c) 2004-2005 Gil Megidish
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include <memory.h>
#include "debug.h"
#include "vm.h"

#define MAX_VARIABLES 256
#define MEMORY_SIZE VM_MEMORY_SIZE

short variables[MAX_VARIABLES];
static unsigned char memory[MEMORY_SIZE]; 

int auxptr;
static int using_aux = 0;
short auxvars[MAX_TASKS*32];

void set_aux_bank(int bank)
{
	if (bank < 0 || bank >= MAX_TASKS)
	{
		bank = 0;
	}

	auxptr = (bank * 32);
}

/** Loads byte from memory offset
    @param offset    address (512K)
    @returns value
*/
unsigned char get_byte(int offset)
{
	if (offset < 0 || offset >= MEMORY_SIZE)
	{
		LOG(("get_byte: offset 0x%x out of range\n", offset));
		return 0;
	}

	return memory[offset];
}

/** Loads word from memory offset
    @param offset    address (512K)
    @return value

    No alignment restrictions
*/
unsigned short get_word(int offset)
{
	if (offset < 0 || offset + 1 >= MEMORY_SIZE)
	{
		LOG(("get_word: offset 0x%x out of range\n", offset));
		return 0;
	}

	return (get_byte(offset) << 8) | get_byte(offset + 1);
}

/** Loads long from memory offset
    @param offset    address (512K)
    @return value

    No alignment restrictions
*/
unsigned long get_long(int offset)
{
	if (offset < 0 || offset + 3 >= MEMORY_SIZE)
	{
		LOG(("get_long: offset 0x%x out of range\n", offset));
		return 0;
	}

	/* cast before shifting: get_word() promotes to int, and shifting a
	 * value with bit 15 set into bit 31 is undefined behaviour */
	return ((unsigned long)get_word(offset) << 16) | get_word(offset + 2);
}

/** Gets variable either from global memory, or from auxiliry memory
    @param var   index in memory

    Note that each variable is 16 bit. in global memory there are 256
    variables, while in auxiliry memory, there are only 32. use 
    toggle_aux() to select between these two memory sets
*/
short get_variable(int var)
{
	if (using_aux)
	{
		int index = auxptr + var;

		if (var < 0 || var >= 32 || index < 0 || index >= MAX_TASKS*32)
		{
			LOG(("get_variable: aux index 0x%x out of range\n", var));
			return 0;
		}

		return auxvars[index];
	}

	if (var < 0 || var >= MAX_VARIABLES)
	{
		LOG(("get_variable: index 0x%x out of range\n", var));
		return 0;
	}

	return variables[var];
}

void set_variable(int var, short value)
{
	if (using_aux)
	{
		int index = auxptr + var;

		if (var < 0 || var >= 32 || index < 0 || index >= MAX_TASKS*32)
		{
			LOG(("set_variable: aux index 0x%x out of range\n", var));
			return;
		}

		auxvars[index] = value;
		return;
	}

	if (var < 0 || var >= MAX_VARIABLES)
	{
		LOG(("set_variable: index 0x%x out of range\n", var));
		return;
	}

	variables[var] = value;
}

/** Copies variables from aux storage to global set
    @param dst_index first element in aux storage
    @param src_index first element in global storage
    @param count     number of elements to copy
*/
void copy_tls_to_global(int dst_index, int src_index, int count)
{
	/* copy variables from the active (TLS) bank to the global set */
	while (count >= 0)
	{
		if (dst_index >= 0 && dst_index < MAX_VARIABLES)
		{
			variables[dst_index] = get_variable(src_index);
		}

		dst_index++;
		src_index++;
		count--;
	}
}

/** Copies variables from global set, to aux storage
    @param dst_index first element in aux storage
    @param src_index first element in global storage
    @param count     number of elements to copy
*/
void copy_global_to_tls(int dst_index, int src_index, int count)
{
	while (count >= 0)
	{
		if (src_index >= 0 && src_index < MAX_VARIABLES)
		{
			set_variable(dst_index, variables[src_index]);
		}

		dst_index++;
		src_index++;
		count--;
	}
}

/** Toggles use of auxiliry memory for getters and setters
    @param toggle  non-zero to use aux memory
    @returns old toggle value
*/
int toggle_aux(int toggle)
{
	int old = using_aux;
	using_aux = toggle;
	return old;
}

/** Returns a pointer to the specific offset in virtual memory
    @param offset  position in bytes from begining of memory
    @returns pointer to that region
*/
unsigned char *get_memory_ptr(int offset)
{
	if (offset < 0)
	{
		LOG(("get_memory_ptr: offset 0x%x out of range\n", offset));
		offset = 0;
	}
	else if (offset >= MEMORY_SIZE)
	{
		LOG(("get_memory_ptr: offset 0x%x out of range\n", offset));
		offset = 0;
	}

	return (unsigned char *)memory + offset;
}

/** Resets VM, zeros all variables and aux memory
*/
void vm_reset()
{
	auxptr = 0;
	using_aux = 0;
	memset(variables, '\0', sizeof(variables));
	memset(auxvars, '\0', sizeof(auxvars));
}
