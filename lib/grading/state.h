#ifndef __GRADING_STATE_H
#define __GRADING_STATE_H

#include <aos/aos.h>

#include "options.h"

extern coreid_t grading_coreid;
extern struct bootinfo *grading_bootinfo;
extern struct grading_options grading_options;
extern const char *grading_proc_name;

#endif /* __GRADING_STATE_H */
