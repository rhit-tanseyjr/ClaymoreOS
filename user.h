#pragma once
#include "common.h"
#define SYS_PUTCHAR 1
int syscall(int arg0, int arg1, int arg2, int sysno);
__attribute__((noreturn)) void exit(void);
void putchar(char ch);


