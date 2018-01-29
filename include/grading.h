#ifndef __GRADING_H
#define __GRADING_H

#include <aos/aos.h>

/* Initialises the grading library within a process.  Takes the process's
 * command-line arguments, and removes anything grading-specific.  Needs to
 * know whether this is 'init' or not, as it may have to look in the bootinfo
 * struct. */
void grading_setup(int *argc, char ***argv, bool init_process);

/* Called to provide the bootinfo pointer to the grading code.  This is called
 * internally on the first core, but must be called separately on the second
 * core, once the bootinfo frame is mapped. */
void grading_set_bootinfo(struct bootinfo *bi);

/* To be called after bootinfo and ram_alloc() are set up, but before entering
 * the event loop, or starting any children. */
void grading_early_init(void);

/* To be called once all init functionality can be expected to work. */
void grading_late_init(void);

/* Panic and spin. */
void grading_panic(const char *fmt, ...);

/* Print with a <grading> banner. */
void grading_printf(const char *fmt, ...);

/* Print without a <grading> banner. */
void grading_printf_nb(const char *fmt, ...);

/* Record test failure. */
void grading_test_fail(const char *test, const char *fmt, ...);

/* Record test success. */
void grading_test_pass(const char *test, const char *fmt, ...);

#endif /* __GRADING_H */
