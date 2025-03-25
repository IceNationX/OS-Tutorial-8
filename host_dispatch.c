#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MEMORY 1024

// Process structure (DO NOT CHANGE FIELD NAMES)
typedef struct proc {
    char name[256];
    int priority;
    pid_t pid;
    int address;
    int memory;
    int runtime;
    bool suspended;
} proc_t;

// Queue node (for FIFO implementation)
typedef struct node {
    proc_t process;
    struct node *next;
} node_t;

// Queue (for priority/secondary queues)
typedef struct {
    node_t *head;
    node_t *tail;
} queue_t;

// Initialize a queue
void queue_init(queue_t *q) {
    q->head = NULL;
    q->tail = NULL;
}

// Add to queue (push)
void push(queue_t *q, proc_t process) {
    node_t *new_node = malloc(sizeof(node_t));
    new_node->process = process;
    new_node->next = NULL;
    
    if (q->tail == NULL) {
        q->head = q->tail = new_node;
    } else {
        q->tail->next = new_node;
        q->tail = new_node;
    }
}

// Remove from queue (pop)
bool pop(queue_t *q, proc_t *process) {
    if (q->head == NULL) return false;
    
    node_t *temp = q->head;
    *process = temp->process;
    q->head = q->head->next;
    
    if (q->head == NULL) {
        q->tail = NULL;
    }
    
    free(temp);
    return true;
}

// Check if queue is empty
bool is_empty(queue_t *q) {
    return q->head == NULL;
}

// Memory management
int avail_mem[MEMORY] = {0};

// Allocate memory block
int allocate_memory(int size) {
    int start = -1;
    int count = 0;
    
    for (int i = 0; i < MEMORY; i++) {
        if (avail_mem[i] == 0) {
            if (count == 0) start = i;
            count++;
            if (count == size) {
                for (int j = start; j < start + size; j++) {
                    avail_mem[j] = 1;
                }
                return start;
            }
        } else {
            count = 0;
            start = -1;
        }
    }
    return -1; // Not enough memory
}

// Free memory
void free_memory(int start, int size) {
    for (int i = start; i < start + size; i++) {
        avail_mem[i] = 0;
    }
}

int main() {
    queue_t priority, secondary;
    queue_init(&priority);
    queue_init(&secondary);

    // Read processes from file
    FILE *file = fopen("processes_q2.txt", "r");
    if (!file) {
        perror("Failed to open processes_q2.txt");
        return 1;
    }

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        proc_t p = {0};
        sscanf(line, "%255[^,],%d,%d,%d", p.name, &p.priority, &p.memory, &p.runtime);
        
        if (p.priority == 0) {
            push(&priority, p);
        } else {
            push(&secondary, p);
        }
    }
    fclose(file);

    // Execute priority-0 processes
    while (!is_empty(&priority)) {
        proc_t p;
        pop(&priority, &p);

        p.address = allocate_memory(p.memory);
        if (p.address == -1) {
            printf("Insufficient memory for %s\n", p.name);
            continue;
        }

        printf("Executing: %s (priority: %d, memory: %d MB, runtime: %d s)\n", 
               p.name, p.priority, p.memory, p.runtime);

        pid_t pid = fork();
        if (pid == 0) {
            char runtime_str[16];
            snprintf(runtime_str, sizeof(runtime_str), "%d", p.runtime);
            execl("./process", "process", runtime_str, NULL);
            perror("execl failed");
            exit(1);
        } else if (pid > 0) {
            p.pid = pid;
            sleep(p.runtime);
            kill(pid, SIGTSTP);
            waitpid(pid, NULL, 0);
            free_memory(p.address, p.memory);
        } else {
            perror("fork failed");
        }
    }

    // Execute secondary processes
    while (!is_empty(&secondary)) {
        proc_t p;
        pop(&secondary, &p);

        p.address = allocate_memory(p.memory);
        if (p.address == -1) {
            push(&secondary, p);
            continue;
        }

        printf("Executing: %s (priority: %d, memory: %d MB, runtime: %d s)\n", 
               p.name, p.priority, p.memory, p.runtime);

        if (p.suspended && p.pid != 0) {
            kill(p.pid, SIGCONT);
        } else {
            pid_t pid = fork();
            if (pid == 0) {
                char runtime_str[16];
                snprintf(runtime_str, sizeof(runtime_str), "%d", p.runtime);
                execl("./process", "process", runtime_str, NULL);
                perror("execl failed");
                exit(1);
            } else if (pid > 0) {
                p.pid = pid;
            } else {
                perror("fork failed");
                continue;
            }
        }

        sleep(1);
        kill(p.pid, SIGTSTP);
        p.runtime--;
        p.suspended = true;

        if (p.runtime <= 1) {
            kill(p.pid, SIGCONT);
            sleep(p.runtime);
            kill(p.pid, SIGINT);
            waitpid(p.pid, NULL, 0);
            free_memory(p.address, p.memory);
        } else {
            push(&secondary, p);
        }
    }

    return 0;
}
