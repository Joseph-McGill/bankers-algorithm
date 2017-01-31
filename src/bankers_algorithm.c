/**
 * Joseph McGill
 * October 2015
 *
 * Resource management project that implements the Banker's algorithm
 * for deadlock avoidance
 **/

#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/queue.h>
#include <ctype.h>

/* Constants for the resources to be managed */
#define R1 13
#define R2 10
#define R3 7
#define R4 12

/* structs */
typedef struct Instruction {
    char id[4];
    int values[4];
} Instruction;

typedef struct Process {
    int id;
    int max_need[4];
    int allocated_resources[4];
    int instruction_counter;
    int current_instruction;
    Instruction instructions[50];
    TAILQ_ENTRY(Process) processes;
    TAILQ_ENTRY(Process) hold_processes;
} Process;


/* Function prototypes */
int build_standby_queue();
int build_max_need_matrix();
int build_allocation_matrix();
int build_need_matrix();
int sleep_process(Process* p);
int request_resources(Process* p, int* resources);
int release_resources(Process* p, int* resources);
int terminate_process(Process* p);
Process create_process(char* data);
bool is_safe_state();
void run_processes();
void execute_instruction(Process* p, Instruction *inst);
void print_matrices();

/* Global variables */
int max_need_matrix[5][4];
int allocation_matrix[5][4];
int need_matrix[5][4];
int available_matrix[4];
Process temp;
Process procs[128];
Instruction instruct[256];
int procs_count = 0;
int instructs_count = 0;

/* Heads for TAILQs */
struct HEADNAME *standby_head;
struct HEADNAME *ready_head;
struct HEADNAME *wait_head;
struct HEADNAME *hold_head;
TAILQ_HEAD(standby_head, Process) standby_queue;
TAILQ_HEAD(ready_head, Process) ready_queue;
TAILQ_HEAD(wait_head, Process) wait_queue;
TAILQ_HEAD(hold_head, Process) hold_queue;

/* Main */
int main(int argc, char **argv) {

    /* variables for loops */
    int i, j, k, l;

    /* initialize queues for storing processes */
    TAILQ_INIT(&ready_queue);
    TAILQ_INIT(&wait_queue);
    TAILQ_INIT(&standby_queue);
    TAILQ_INIT(&hold_queue);

    /* build the initial resource availability matrix */
    available_matrix[0] = R1;
    available_matrix[1] = R2;
    available_matrix[2] = R3;
    available_matrix[3] = R4;

    /* build the standby queue */
    if (build_standby_queue() == -1) {
        printf("Could not parse data");
        return 0;
    }

    /* build the ready queue */
    Process *p;
    for (i = 0; i < 5; i++) {
        p = TAILQ_FIRST(&standby_queue);
        TAILQ_REMOVE(&standby_queue, p, processes);
        TAILQ_INSERT_TAIL(&hold_queue, p, hold_processes);
        TAILQ_INSERT_TAIL(&ready_queue, p, processes);
    }

    /* build the initial resource availability matrix */
    available_matrix[0] = R1;
    available_matrix[1] = R2;
    available_matrix[2] = R3;
    available_matrix[3] = R4;

    /* build the initial allocation matrix */
    build_allocation_matrix();

    /* build the initial max_need_matrix */
    build_max_need_matrix();

    /* build the initial need_matrix */
    build_need_matrix();

    /* run the processes */
    run_processes();

    return 0;
}

/* Function to create a process from a character array */
Process create_process(char* data) {

    /* variables for loops */
    int i, j;
    int index = 0;

    /* skips until the 'D' in 'ID' */
    while (data[index] != 'D') {
        index++;
    }
    index++;

    /* skips whitespace until the ID number */
    while (isspace(data[index])) {
        index++;
    }

    /* temp variables for storing the read ID number as chars */
    char id_num[5] = {'\0', '\0', '\0', '\0', '\0'};
    i = 0;

    /* store each char as long as it is a digit */
    while (isdigit(data[index])) {
        id_num[i] = data[index];
        index++;
        i++;
    }

    /* skip whitespace until next instruction */
    while (isspace(data[index]) || data[index] == 'M' || data[index] == 'N') {
        index++;
    }

    int temp_max_need[4];

    /* loop to get the max need for the process */
    for(j = 0; j < 4; j++) {

        /* variables for storing the max need as chars */
        char temp_max_need_chars[4] = {'\0', '\0', '\0', '\0'};
        i = 0;

        /* skip whitespace until next number */
        while (isspace(data[index])) {
            index++;
        }

        /* get the next digit */
        while (isdigit(data[index])) {
            temp_max_need_chars[i] = data[index];
            index++;
            i++;
        }

        /* convert the max need from chars to an int */
        temp_max_need[j] = atoi(temp_max_need_chars);
    }

    /* set max need of process */
    temp.max_need[0] = temp_max_need[0];
    temp.max_need[1] = temp_max_need[1];
    temp.max_need[2] = temp_max_need[2];
    temp.max_need[3] = temp_max_need[3];

    /* variables to store the instructions */
    int instruction_counter = 0;
    Instruction inst[50];

    /* loop until buffer 'END', the final instruction */
    while (true) {

        Instruction temp_instruction;

        char temp_inst_id[4] = {'\0', '\0', '\0', '\0'};

        while (isspace(data[index])) {
            index++;
        }

        int k = 0;
        while (isalpha(data[index])) {

            temp_inst_id[k] = data[index];
            k++;
            index++;
        }

        /* check if the beginning of the instruction is 'E',
         * signaling the final instruction */
        if (temp_inst_id[0] == 'E') {

            /* create the final instruction */
            temp_instruction = (Instruction) {
                .id[0] = 'E',
                .id[1] = 'N',
                .id[2] = 'D',
                .id[3] = '\0',
                .values[0] = 0,
                .values[1] = 0,
                .values[2] = 0,
                .values[3] = 0
            };

            /* store the final instruction and exit the loop */
            temp.instructions[instruction_counter] = temp_instruction;
            instruction_counter++;
            break;
        }

        /* array to store numbers from buffer */
        int temp_numbers[4] = {0, 0, 0, 0};

        /* loop through buffer contents 4 times to get the
         * 4 associated numbers */
        for(j = 0; j < 4; j++) {

            /* variables to parse buffer contents */
            i = 0;
            char temp_number_chars[4] = {'\0', '\0', '\0', '\0'};

            /* skip whitespace */
            while (isspace(data[index])) {
                index++;
            }

           /* read input while it is a number */
            while (isdigit(data[index])) {
                temp_number_chars[i] = data[index];
                index++;
                i++;
            }

            /* convery chars to ints */
            temp_numbers[j] = atoi(temp_number_chars);
        }

        /* create a temporary instruction */
        temp_instruction = (Instruction) {
            .id[0] = temp_inst_id[0],
            .id[1] = temp_inst_id[1],
            .id[2] = temp_inst_id[2],
            .id[3] = temp_inst_id[3],
            .values[0] = temp_numbers[0],
            .values[1] = temp_numbers[1],
            .values[2] = temp_numbers[2],
            .values[3] = temp_numbers[3]
        };

        /* store each instruction into an array of instructions */
        temp.instructions[instruction_counter] = temp_instruction;
        instruction_counter++;
    }


    /* populate the temp process' data */
    temp.id = atoi(id_num);
    temp.instruction_counter = instruction_counter;

    return temp;
}

/* Function to terminate a process and release all resources allocated to it */
int terminate_process(Process* p) {

    int i, j;
    Process *temp_process;

    /* release allocated resources */
    for (i = 0; i < 4; i++) {
        available_matrix[i] = available_matrix[i] + p->allocated_resources[i];
        p->allocated_resources[i] = 0;
    }

    /* remove from ready_queue */
    TAILQ_REMOVE(&ready_queue, p, processes);
    TAILQ_REMOVE(&hold_queue, p, hold_processes);

    /* return if no more processes to load */
    if (TAILQ_EMPTY(&standby_queue)) {
        return 0;
    }

    /* add next process to the ready_queue */
    temp_process = TAILQ_FIRST(&standby_queue);
    TAILQ_REMOVE(&standby_queue, temp_process, processes);
    TAILQ_INSERT_TAIL(&hold_queue, temp_process, hold_processes);
    TAILQ_INSERT_TAIL(&ready_queue, temp_process, processes);

    /* rebuild all matrices */
    build_max_need_matrix();
    build_allocation_matrix();
    build_need_matrix();

    return 0;
}

/* Function to run processes from the ready_queue */
void run_processes() {

    Process* p;

    /* loop until ready_queue is empty */
    while(true) {

        /* get the first entry in the ready queue */
        if (TAILQ_EMPTY(&ready_queue)) {

            return;
        } else {

            p = TAILQ_FIRST(&ready_queue);
        }

        /* execute the next instruction */
        execute_instruction(p, &p->instructions[p->current_instruction]);

    }
}

/* Function to execute an instruction */
void execute_instruction(Process* p, Instruction *inst) {

    /* determine the instruction and execute accordingly */
    if (inst->id[0] == 'R' && inst->id[1] == 'Q') {

        /* if request is granted, increment current_instruction */
        if(request_resources(p, inst->values) == 0) {
            p->current_instruction += 1;
            return;
        }

    }
    else if (inst->id[0] == 'R' && inst->id[1] == 'L') {

        /* release desired resources */
        release_resources(p, inst->values);
        p->current_instruction += 1;
        return;
    }
    else if (inst->id[0] == 'S' && inst->id[1] == 'L') {

        /* sleep the current process */
        sleep_process(p);
        p->current_instruction += 1;
        return;
    }
    else if (inst->id[0] == 'E' && inst->id[1] == 'N') {

        /* terminate the current process */
        terminate_process(p);
        return;
    }

    return;
}

/* Function to build the standby_queue */
int build_standby_queue() {

    int bytes_read;
    int i;
    char c;
    char buffer[512];

    /* parse the file one processes at a time */
    while(true) {

        i = 0;
        for (i; i < 512; i++) {
            buffer[i] = '\0';
        }

        i = 0;

        /* read each process one byte at a time */
        while((bytes_read = read(0, &c, 1)) > 0 && c != 'E') {
            buffer[i] = c;
            i++;
        }

        /* return if no bytes read (EOF) */
        if (bytes_read == 0) {
            return 0;
        }

        /* place END in the buffer for last instruction */
        if (c == 'E') {
            buffer[i    ] = 'E';
            buffer[i + 1] = 'N';
            buffer[i + 2] = 'D';

            /* advance the reader by 2 characters */
            bytes_read = read(0, &c, 1);
            bytes_read = read(0, &c, 1);
        }

        /* create and store process in queue */
        procs[procs_count] = create_process(buffer);
        TAILQ_INSERT_TAIL(&standby_queue, &procs[procs_count], processes);
        procs_count++;

    }

    /* immediately return -1 if loop is not entered */
    return -1;
}

/* Function to 'sleep' a process */
int sleep_process(Process* p) {


    /* remove the process from the queue */
    TAILQ_REMOVE(&ready_queue, p, processes);

    /* reinitialize the queue if empty */
    if (TAILQ_EMPTY(&ready_queue)) {

        TAILQ_INIT(&ready_queue);
    }

    /* reinsert the process at the tail of the queue */
    TAILQ_INSERT_TAIL(&ready_queue, p, processes);

    return 0;
}

/* Function to request resources for a process */
int request_resources(Process* p, int* resources) {

    int i, j;

    /* if there are enough resources to satisfy the request,
     * run safe check. Otherwise move to wait queue */
    if (resources[0] <= available_matrix[0] &&
        resources[1] <= available_matrix[1] &&
        resources[2] <= available_matrix[2] &&
        resources[3] <= available_matrix[3]) {

        /* make allocation */
        for (i = 0; i < 4; i++) {
            p->allocated_resources[i] = p->allocated_resources[i] +
                                        resources[i];

            available_matrix[i] = available_matrix[i] - resources[i];
        }

        /* rebuild matrices */
        build_allocation_matrix();
        build_need_matrix();

        /* roll back if safe state is not reached */
        if (!is_safe_state()) {

            /* unallocate resources */
            for (i = 0; i < 4; i++) {
                p->allocated_resources[i] = p->allocated_resources[i]
                                            - resources[i];
                available_matrix[i] = available_matrix[i] + resources[i];
            }

            /* rebuild matrices */
            build_allocation_matrix();
            build_max_need_matrix();
            build_need_matrix();

            /* print info */
            printf("Request of job No. %d for resources: %d %d %d %d "
                   "cannot be satisfied\n",
                    p->id, resources[0], resources[1],
                    resources[2], resources[3]);
            print_matrices();

            /* place in wait queue */
            Process *temp_process = TAILQ_FIRST(&ready_queue);
            TAILQ_REMOVE(&ready_queue, temp_process, processes);
            TAILQ_INSERT_TAIL(&wait_queue, temp_process, processes);

            return -1;
        }

     } else {

         /* move to wait queue if resources are not available */
         Process *temp_process = TAILQ_FIRST(&ready_queue);
         TAILQ_REMOVE(&ready_queue, temp_process, processes);
         TAILQ_INSERT_TAIL(&wait_queue, temp_process, processes);
         return -1;
     }


    return 0;
}

/* Function to release resrouces from a process */
int release_resources(Process* p, int* resources) {

    Process *temp_process;

    /* if the process tried to release more resources than it has,
     * terminate the process */
    if (resources[0] <= p->allocated_resources[0] &&
        resources[1] <= p->allocated_resources[1] &&
        resources[2] <= p->allocated_resources[2] &&
        resources[3] <= p->allocated_resources[3]) {

        /* release resouces */
        available_matrix[0] = available_matrix[0] + resources[0];
        available_matrix[1] = available_matrix[1] + resources[1];
        available_matrix[2] = available_matrix[2] + resources[2];
        available_matrix[3] = available_matrix[3] + resources[3];

        p->allocated_resources[0] = p->allocated_resources[0] - resources[0];
        p->allocated_resources[1] = p->allocated_resources[1] - resources[1];
        p->allocated_resources[2] = p->allocated_resources[2] - resources[2];
        p->allocated_resources[3] = p->allocated_resources[3] - resources[3];


        /* rebuild allocation and need matrices */
        build_allocation_matrix();
        build_max_need_matrix();
        build_need_matrix();

        /* check wait queue by moving the processes to the ready queue */
        while (!TAILQ_EMPTY(&wait_queue)) {

            temp_process  = TAILQ_FIRST(&wait_queue);
            TAILQ_REMOVE(&wait_queue, temp_process, processes);
            TAILQ_INSERT_TAIL(&ready_queue, temp_process, processes);
        }

        TAILQ_INIT(&wait_queue);

    } else {

        /* abnormally terminate the process */
        terminate_process(p);

        /* check wait queue by moving the processes to the ready queue */
        while (!TAILQ_EMPTY(&wait_queue)) {

            temp_process  = TAILQ_FIRST(&wait_queue);
            TAILQ_REMOVE(&wait_queue, temp_process, processes);
            TAILQ_INSERT_TAIL(&ready_queue, temp_process, processes);
        }

        /* reinitialize queue after emptying */
        TAILQ_INIT(&wait_queue);

        /* return -1, indicated error */
        return -1;
    }

    return 0;
}

/* Function to check if allocating resources to a process
 * results in a safe state */
bool is_safe_state() {

    int i, j, k, l;
    bool is_safe[5] = {false, false, false, false, false};

    /* local copies of matrices to test */
    int copy_need[5][4];
    int copy_allocation[5][4];
    int copy_available[4];

    /* copy matrices */
    for (i = 0; i < 5; i++) {
        for (j = 0; j < 4; j++) {
            copy_need[i][j] = need_matrix[i][j];
            copy_allocation[i][j] = allocation_matrix[i][j];
        }
    }

    for (i = 0; i < 4; i++) {
        copy_available[i] = available_matrix[i];
    }

    /* run loop 5 times */
    for (i = 0; i < 5; i++) {

        /* step through each process */
        for (j = 0; j < 5; j++) {

            /* enter if process is not safe */
            if (is_safe[j] == false) {

                if (copy_need[j][0] <= copy_available[0] &&
                    copy_need[j][1] <= copy_available[1] &&
                    copy_need[j][2] <= copy_available[2] &&
                    copy_need[j][3] <= copy_available[3]) {

                    /* release the resrouces */
                    copy_available[0] = copy_available[0] +
                                        copy_allocation[j][0];

                    copy_available[1] = copy_available[1] +
                                        copy_allocation[j][1];

                    copy_available[2] = copy_available[2] +
                                        copy_allocation[j][2];

                    copy_available[3] = copy_available[3] +
                                        copy_allocation[j][3];

                    /* set allocated resources to 0 */
                    copy_allocation[j][0] = 0;
                    copy_allocation[j][1] = 0;
                    copy_allocation[j][2] = 0;
                    copy_allocation[j][3] = 0;

                    /* process is safe */
                    is_safe[j] = true;
                }
            }
        }
    }

    /* return true if system is in a safe state, and false otherwise */
    if (is_safe[0] == true && is_safe[1] == true &&
        is_safe[2] == true && is_safe[3] == true &&
        is_safe[4] == true) {
        return true;

    } else return false;
}

/* Function to build the max_need_matrix using the ready_queue */
int build_max_need_matrix() {

    /* variables to store a process */
    int i = 0;
    int j;
    Process *p;

    /* loop through the ready_queue to build the max_need_matrix */
    TAILQ_FOREACH(p, &hold_queue, hold_processes) {
        for(j = 0; j < 4; j++) {
            max_need_matrix[i][j] = p->max_need[j];
        }
        i++;
    }

    return 0;
}

/* Function to build the allocation_matrix using the ready_queue */
int build_allocation_matrix() {

    /* variables to store a process */
    int i = 0;
    int j;
    Process *p;

    /* loop through the ready_queue to build the allocation_matrix */
    TAILQ_FOREACH(p, &hold_queue, hold_processes) {
        for(j = 0; j < 4; j++) {
            allocation_matrix[i][j] = p->allocated_resources[j];
        }
        i++;
    }

    return 0;
}

/* Function to build the need_matrix using the ready_queue */
int build_need_matrix() {

    /* variables to store a process */
    int i = 0;
    int j;
    Process *p;

    /* loop through the ready_queue to build the need_matrix */
    TAILQ_FOREACH(p, &hold_queue, hold_processes) {
        for(j = 0; j < 4; j++) {
            need_matrix[i][j] = max_need_matrix[i][j] - allocation_matrix[i][j];
        }
        i++;
    }

    return 0;
}

/* Function to print the relevant matrices */
void print_matrices() {


    int i, j, k;
    printf("\n");

    /* print available matrix */
    printf("Available Matrix:\n");
    for (i = 0; i < 4; i++) {
        printf("%d ", available_matrix[i]);
    }

    /* print allocation matrix */
    printf("\n\nAllocation Matrix:");
    for (i = 0; i < 5; i++) {
        printf("\n");
        for (j = 0; j < 4; j++) {
            printf("%d ", allocation_matrix[i][j]);
        }
    }

    /* print max need matrix */
    printf("\n\nMax Need Matrix:");
    for (i = 0; i < 5; i++) {
        printf("\n");
        for (j = 0; j < 4; j++) {
            printf("%d ", max_need_matrix[i][j]);
        }
    }

    /* print need matrix */
    printf("\n\nNeed Matrix:");
    for (i = 0; i < 5; i++) {
        printf("\n");
        for (j = 0; j < 4; j++) {
            printf("%d ", need_matrix[i][j]);
        }
    }

    printf("\n----------------------------------"
           "-------------------------------\n");
}
