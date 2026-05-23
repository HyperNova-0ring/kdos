#ifndef KEYBOARD_H
#define KEYBOARD_H

#ifndef __x86_64__
#error "keyboard.h is only for x86 implementation"
#endif

void keyboard_init(void);
int  keyboard_getchar(void);   /* blocking poll — returns ASCII character */

#endif
