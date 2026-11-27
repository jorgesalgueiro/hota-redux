#ifndef __DECODE_INCLUDED__
#define __DECODE_INCLUDED__

#define INVALID_PC        -1

unsigned char next_pc(void);
unsigned short next_pc_word(void);

int decode(int current_task, int start_pc);

#endif
