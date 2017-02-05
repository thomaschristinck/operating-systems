#include "jobs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Struct to hold job information (name and process ID)
typedef struct
{
    char* name;
    pid_t pid;
} job;

struct {
    size_t n_jobs;
    const size_t max_jobs;

    job data[MAX_JOBS];
} joblist = {0, MAX_JOBS, {}};

void* checked_malloc(size_t size)
{
    void *result = malloc(size);
    if (result == NULL)
    {
        fprintf(stderr, "malloc(size = %zu) failed, aborting.\n", size);
        exit(EXIT_FAILURE);
    }

    return result;
}

job construct_job(const char * name, pid_t pid)
{
    size_t len = strlen(name);
    job output;

    output.name = (char*) checked_malloc(len);
    strcpy(output.name, name);
    output.pid = pid;

    joblist.n_jobs++;
    return output;
}

void destruct_job(job *input)
{
    free(input->name);
    input->name = 0;
    input->pid = 0;

    joblist.n_jobs--;
}

// check currently running background tasks for completion, remove from list
// if done
void update_jobs()
{
    for (size_t i = 0; i < joblist.max_jobs; i++)
    {
        if (joblist.data[i].name == NULL)
            continue;

        // if process exited
        if (waitpid(joblist.data[i].pid, NULL, WNOHANG))
            destruct_job(&joblist.data[i]);
    }
}

// returns ID or -1 if failed
int to_background(const char * name, pid_t pid)
{
    update_jobs();

    if (joblist.n_jobs == joblist.max_jobs)
    {
        fprintf(stderr, "No slots left for background job '%s', "
            "hit maximum of %zu.\n", name, joblist.max_jobs);
        return -1;
    }

    for (size_t i = 0; i < joblist.max_jobs; i++)
    {
        if (!joblist.data[i].name)
        {
            joblist.data[i] = construct_job(name, pid);
            return (int) i;
        }
    }

    fprintf(stderr, "Failed to find slot for background job '%s' despite "
            "recorded number of jobs (%zu) being less than maximum (%zu).\n",
        name, joblist.n_jobs, joblist.max_jobs);

    return -1;
}

bool to_foreground(const char * id_string)
{
    update_jobs();

    char *ptr;
    long id = strtol(id_string, &ptr, 10);

    if ((ptr == id_string)                 || // if strtol failed
        (id < 0 && id >= joblist.max_jobs) || // if the id in out of range
        (joblist.data[id].name == NULL))      // if there is no job at the id
    {
        fprintf(stderr, "Invalid job id '%s', could not resume.\n", id_string);
        return false;
    }

    // wait
    printf("Moving [%ld] %s to foreground.\n", id, joblist.data[id].name);
    waitpid(joblist.data[id].pid, NULL, 0);

    // then remove the job
    destruct_job(&joblist.data[id]);
    return true;
}

void print_jobs()
{
    update_jobs();

    if (joblist.n_jobs == 0)
    {
        printf("No active jobs\n");
        return;
    }

    // We want to print a list of all non-NULL jobs
    for (size_t i = 0; i < joblist.max_jobs; i++)
        if (joblist.data[i].name != NULL)
            printf("[%zu] %s\n", i, joblist.data[i].name);

}