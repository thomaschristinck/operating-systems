#ifndef SHELL_JOBS_H
#define SHELL_JOBS_H

#include <stdbool.h>
#include <sys/wait.h>

#define MAX_JOBS 10

void* checked_malloc(size_t size);

// Returns ID or -1 if failed
int to_background(const char * name, pid_t pid);

// Returns boolean success
bool to_foreground(const char * id_string);

// Prints jobs to stdout
void print_jobs();

#endif // SHELL_JOBS_H