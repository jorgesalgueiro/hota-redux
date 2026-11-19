#ifndef __DEBUG_INCLUDED__
#define __DEBUG_INCLUDED__

#include <stdio.h>

extern int debug_flag;

/* Logging is always compiled in and selected at runtime with --debug.
 * Keeping it in release builds makes user bug reports much easier. */
#define LOG(x) if (debug_flag) { printf x; }

void mark_opcode(int opcode);
void print_hex(unsigned char *ptr, int length);

#endif
