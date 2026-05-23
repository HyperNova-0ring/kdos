#include "../kernel/module_abi.h"

MODULE_HEADER("COMMAND.KERN", "1.0");

/* ── string helpers ──────────────────────────────────────── */

static int str_eq(const char* a, const char* b) {
    while (*a && *b) { if (*a != *b) return 0; a++; b++; }
    return *a == *b;
}

static int str_starts(const char* s, const char* prefix) {
    while (*prefix) { if (*s++ != *prefix++) return 0; }
    return 1;
}

/* ── line reader ─────────────────────────────────────────── */

#define LINE_MAX 128

static int readline(const kst_t* kst, char* buf) {
    int len = 0;
    for (;;) {
        int c = kst->console.getchar();

        /* Both \r (serial terminals) and \n terminate the line */
        if (c == '\r' || c == '\n') {
            kst->console.putchar('\n');
            buf[len] = '\0';
            return len;
        }

        if (c == '\b' || c == 0x7F) {
            if (len > 0) {
                len--;
                kst->console.print("\b \b");
            }
            /* len == 0: send nothing — cursor stays at prompt position */
            continue;
        }

        if (c >= 0x20 && len < LINE_MAX - 1) {
            buf[len++] = (char)c;
            kst->console.putchar((char)c);
        }
    }
}

/* ── commands ────────────────────────────────────────────── */

static void cmd_help(const kst_t* kst) {
    kst->console.print(
        "Available commands:\n"
        "  help   — show this help\n"
        "  clear  — clear the screen\n"
        "  halt   — halt the system\n"
    );
}

static void cmd_unknown(const kst_t* kst, const char* cmd) {
    kst->console.print("Unknown command: ");
    kst->console.print(cmd);
    kst->console.putchar('\n');
}

static void dispatch(const kst_t* kst, const char* line) {
    if (line[0] == '\0')  return;
    if (str_eq(line, "help"))  { cmd_help(kst);  return; }
    if (str_eq(line, "clear")) { kst->console.clear(); return; }
    if (str_eq(line, "halt"))  { kst->sys.panic("halt requested"); }
    (void)str_starts;
    cmd_unknown(kst, line);
}

/* ── entry ───────────────────────────────────────────────── */

__attribute__((section(".text.entry")))
void module_main(const kst_t* kst) {
    char line[LINE_MAX];

    kst->console.clear();
    kst->console.print("KDOS COMMAND.KERN 1.0\nType 'help' for available commands.\n\n");

    for (;;) {
        kst->console.print("> ");
        readline(kst, line);
        dispatch(kst, line);
    }
}
