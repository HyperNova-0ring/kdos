#ifndef MODULES_H
#define MODULES_H

#include "module_abi.h" 

#define MAX_MODULES 16

typedef enum {
    MODULE_STATE_ABSENT  = 0,
    MODULE_STATE_LOADED  = 1,   /* loaded in memory, not yet run */
    MODULE_STATE_RUNNING = 2,   /* main() has been called        */
    MODULE_STATE_FAILED  = 3,
} module_state_t;

typedef struct {
    uintptr_t start;   // Physical Entry
    uintptr_t end;
    char cmdline[64];  // cmdline of module
    module_state_t state;
    module_header_t* header;  
} module_t;

typedef struct {
    module_t modules[MAX_MODULES];
    uint32_t count;
} module_list_t;

// module utilities
void      modules_register(uintptr_t start, uintptr_t end, const char* cmdline);
uint32_t  modules_count(void);
module_t* modules_find(const char* name);
module_list_t* modules_get_all(void);

// run startup/utility modules (skips entry shells)
void modules_run_all(void);

// try COMMAND.KERN, then command.com; print message and return if neither found
void modules_launch_entry(void);

#endif