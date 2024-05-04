#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/types.h>

// Global variables
typedef struct shared_vars {
    FILE *file; // the output file
    int *A; // number of lines in the output file/actions
    int L_count; // total number of skiers
    int *L_boarded; // number of skiers onboard
    int *L_skiing; // number of skiers already skiing
    int Z_count; // number of stops
    int *stops_waiting; // number of skiers at each stop
    int K_capacity; // bus capacity
    int skier_max_time; // max time that a skier waits before going to a bus stop
    int bus_max_time; // max time that the skibus drives to the next bus stop
    sem_t *bus_mutex; // mutex for the skibus (if it is on final stop or not)
    sem_t *all_skiers_finished; // mutex for skiers
    sem_t *output_mutex; // mutex for printing output
    sem_t **stops_mutex; // mutexes for all stops (whether or not bus is on stop)
} shared_vars;
shared_vars *shared_t;

// Function prototypes
void struct_init();
void struct_destroy();
void semaphore_init();
void semaphore_destroy();
void map_memory();
void unmap_memory();
void custom_print(char *, ...);
void bus();
void skier(int);
void rand_sleep(int);


int main(int argc, char *argv[]) {
    
    // Argument count checking
    if(argc != 6) {
        fprintf(stderr, "ERROR: Wrong argument count.\n");
        return 1;
    }

    // **********Argument parsing**********
    
    char *endptr = NULL; // for the strtol function

    // ----------skiers----------
    int L; // skier
    // from the command line
    L = strtol(argv[1], &endptr, 10);
    // check if the argument is valid
    if(strlen(endptr) > 0) {
        fprintf(stderr, "ERROR: Invalid L argument.\n");
        return 1;
    }
    // checks if L is in range
    if(L >= 20000 || L < 1) {
        fprintf(stderr, "ERROR: L value is out of range!\n");
        return 1;
    }

    // ----------stops----------
    int Z; // number of boarding stops
    Z = strtol(argv[2], &endptr, 10);
    if(strlen(endptr) > 0) {
        fprintf(stderr, "ERROR: Invalid Z argument.\n");
        return 1;
    }
    // checks if Z is in range
    if((Z <= 0) || (Z > 10)) {
        fprintf(stderr, "ERROR: Z value is out of range!\n");
        return 1;
    }

    // ----------capacity----------
    int K; // skibus capacity
    K = strtol(argv[3], &endptr, 10);
    if(strlen(endptr) > 0) {
        fprintf(stderr, "ERROR: Invalid K argument.\n");
        return 1;
    }
    // checks if K is in range
    if((K < 10) || (K > 100)) {
        fprintf(stderr, "ERROR: K value is out of range!\n");
        return 1;
    }

    // ----------skier_time----------
    int TL; // max time in micro seconds that a skier waits before he comes to a bus stop
    TL = strtol(argv[4], &endptr, 10);
    if(strlen(endptr) > 0) {
        fprintf(stderr, "ERROR: Invalid TL argument.\n");
        return 1;
    }
    // checks if TL is in range
    if((TL < 0) || (TL > 10000)) {
        fprintf(stderr, "ERROR: TL value is out of range!\n");
        return 1;
    }

    // ----------stop_time----------
    int TB; // max time of bus ride between two stops
    TB = strtol(argv[5], &endptr, 10);
    if(strlen(endptr) > 0) {
        fprintf(stderr, "ERROR: Invalid TB argument.\n");
        return 1;
    }
    // checks if TB is in range
    if((TB < 0) || (TB > 1000)) {
        fprintf(stderr, "ERROR: TB value is out of range!\n");
        return 1;
    }

    // **********End of argument parsing**********

    // initialize random seed
    srand(time(NULL));

    map_memory();
    struct_init();

    // initialize shared variables
    shared_t -> L_count = L;
    shared_t -> Z_count = Z;
    shared_t -> K_capacity = K;
    shared_t -> skier_max_time = TL;
    shared_t -> bus_max_time = TB;

    shared_t -> stops_waiting = malloc(sizeof(int) * Z);
    if(shared_t -> stops_waiting == NULL) {
        fprintf(stderr, "ERROR: Failed to allocate memory for stops.\n");
        struct_destroy();
        return 1;
    }
    // initialize stops to 0
    for(int i = 0; i < Z; i++) {
        shared_t -> stops_waiting[i] = 0;
    }

    pid_t bus_id = fork();
    if(bus_id == 0) {
        bus();
    }
    for(int i = 0; i < shared_t -> L_count; i++) {
        pid_t skier_id = fork();
        if(skier_id == 0) {
            skier(i+1);
        }
    }

    free(shared_t -> stops_waiting);
    struct_destroy();

    return 0;
}

// **********Function definitions**********

// initializes the struct of global/shared variables
void struct_init() {
    shared_t = malloc(sizeof(shared_vars));
    if(shared_t == NULL) {
        fprintf(stderr, "ERROR: Unable to allocate memory for global variables.\n");
        exit(1);
    }
    // initializes all global variables
    shared_t -> A = malloc(sizeof(int));
    if(shared_t -> A == NULL) {
        fprintf(stderr, "ERROR: Memory allocation failed.\n");
        struct_destroy();
        exit(1);
    }
    *(shared_t -> A) = 1;
    shared_t -> L_count = 0;
    shared_t -> L_boarded = malloc(sizeof(int));
    if(shared_t -> A == NULL) {
        fprintf(stderr, "ERROR: Memory allocation failed.\n");
        struct_destroy();
        exit(1);
    }
    *(shared_t -> L_boarded) = 0;
    shared_t -> L_skiing = malloc(sizeof(int));
    if(shared_t -> A == NULL) {
        fprintf(stderr, "ERROR: Memory allocation failed.\n");
        struct_destroy();
        exit(1);
    }
    *(shared_t -> L_skiing) = 0;
    shared_t -> Z_count = 0;
    shared_t -> skier_max_time = 0;
    shared_t -> bus_max_time = 0;
    shared_t -> K_capacity = 0;
    srand(time(NULL));

    // attempts to open a file for output
    shared_t -> file = fopen("proj2.out", "w");
    if(shared_t -> file == NULL) {
        fprintf(stderr, "ERROR: File failed to open\n");
        struct_destroy();
        exit(1);
    }
}

void struct_destroy() {
    unmap_memory();
    if(shared_t -> file != NULL) {
        fclose(shared_t -> file);
    }

    if(shared_t -> stops_waiting != NULL) {
        free(shared_t -> stops_waiting);
    }

    semaphore_destroy();
    
    if(shared_t != NULL) {
        free(shared_t);
    }
}

void semaphore_init() {
    // Initialize bus_mutex semaphore
    shared_t -> bus_mutex = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0);
    if(shared_t -> bus_mutex == MAP_FAILED) {
        fprintf(stderr, "ERROR: Memory mapping failed.\n");
        struct_destroy();
        exit(1);
    }
    if(sem_init(shared_t -> bus_mutex, 1, 0) == -1) {
        fprintf(stderr, "ERROR: Failed to initialize a semaphore!\n");
        struct_destroy();
        exit(1);
    }

    // Initialize all_skiers_finished semaphore
    shared_t -> all_skiers_finished = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0);
    if(shared_t -> all_skiers_finished == MAP_FAILED) {
        fprintf(stderr, "ERROR: Memory mapping failed.\n");
        struct_destroy();
        exit(1);
    }
    if(sem_init(shared_t -> all_skiers_finished, 1, 0) == -1) {
        fprintf(stderr, "ERROR: Failed to initialize a semaphore!\n");
        struct_destroy();
        exit(1);
    }

    // Initialize output_mutex semaphore
    shared_t -> output_mutex = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0);
    if(shared_t -> output_mutex == MAP_FAILED) {
        fprintf(stderr, "ERROR: Memory mapping failed.\n");
        struct_destroy();
        exit(1);
    }
    if(sem_init(shared_t -> output_mutex, 1, 0) == -1) {
        fprintf(stderr, "ERROR: Failed to initialize a semaphore!\n");
        struct_destroy();
        exit(1);
    }

    // Allocate and initialize stops_mutex array of semaphores
    shared_t -> stops_mutex = mmap(NULL, sizeof(sem_t *) * shared_t -> Z_count, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0);
    if(shared_t -> stops_mutex == MAP_FAILED) {
        fprintf(stderr, "ERROR: Memory mapping failed.\n");
        struct_destroy();
        exit(1);
    }
    for(int i = 0; i < shared_t -> Z_count; i++) {
        shared_t -> stops_mutex[i] = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0);
        if(shared_t -> stops_mutex[i] == MAP_FAILED) {
            fprintf(stderr, "ERROR: Memory mapping failed.\n");
            struct_destroy();
            exit(1);
        }
        if(sem_init(shared_t -> stops_mutex[i], 1, 0) == -1) {
            fprintf(stderr, "ERROR: Failed to initialize a semaphore!\n");
            struct_destroy();
            exit(1);
        }
    }
}


void semaphore_destroy() {
    if(shared_t -> bus_mutex != NULL) {
        sem_destroy(shared_t -> bus_mutex);
    }
    if(shared_t -> all_skiers_finished != NULL) {
        sem_destroy(shared_t -> all_skiers_finished);
    }
    if(shared_t -> output_mutex != NULL) {
        sem_destroy(shared_t -> output_mutex);
    }
    if(shared_t -> stops_mutex != NULL) {
        for(int i = 0; i < (shared_t -> Z_count) - 1; i++) {
            if(shared_t -> stops_mutex[i] != NULL) {
                sem_destroy(shared_t -> stops_mutex[i]);
            }
        }
    }   

    if(shared_t -> stops_mutex != NULL) {
        free(shared_t -> stops_mutex);
    } 
}

void map_memory() {
    shared_t = (shared_vars *)mmap(NULL, sizeof(shared_vars), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0);
    if(shared_t == MAP_FAILED) {
        fprintf(stderr, "ERROR: Memory mapping failed.\n");
        struct_destroy();
        exit(1);
    }
    shared_t -> A = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0);
    if(shared_t -> A == MAP_FAILED) {
        fprintf(stderr, "ERROR: Memory mapping failed.\n");
        struct_destroy();
        exit(1);
    }
    shared_t -> L_boarded = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0);
    if(shared_t -> L_boarded == MAP_FAILED) {
        fprintf(stderr, "ERROR: Memory mapping failed.\n");
        struct_destroy();
        exit(1);
    }
    shared_t -> L_skiing = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0);
    if(shared_t -> L_skiing == MAP_FAILED) {
        fprintf(stderr, "ERROR: Memory mapping failed.\n");
        struct_destroy();
        exit(1);
    }
    shared_t -> stops_waiting = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0);
    if(shared_t -> stops_waiting == MAP_FAILED) {
        fprintf(stderr, "ERROR: Memory mapping failed.\n");
        struct_destroy();
        exit(1);
    }
    shared_t -> bus_mutex = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0);
    if(shared_t -> bus_mutex == MAP_FAILED) {
        fprintf(stderr, "ERROR: Memory mapping failed.\n");
        struct_destroy();
        exit(1);
    }
    shared_t -> all_skiers_finished = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0);
    if(shared_t -> all_skiers_finished == MAP_FAILED) {
        fprintf(stderr, "ERROR: Memory mapping failed.\n");
        struct_destroy();
        exit(1);
    }
    shared_t -> output_mutex = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0);
    if(shared_t -> output_mutex == MAP_FAILED) {
        fprintf(stderr, "ERROR: Memory mapping failed.\n");
        struct_destroy();
        exit(1);
    }
    shared_t -> stops_mutex = mmap(NULL, sizeof(sem_t *), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0);
    if(shared_t -> stops_mutex == MAP_FAILED) {
        fprintf(stderr, "ERROR: Memory mapping failed.\n");
        struct_destroy();
        exit(1);
    }
    for(int i = 0; i < shared_t -> Z_count; i++) {
        shared_t -> stops_mutex[i] = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0);
        if(shared_t -> stops_mutex[i] == MAP_FAILED) {
            fprintf(stderr, "ERROR: Memory mapping failed.\n");
            struct_destroy();
            exit(1);
        }
    }
}

void unmap_memory() {
    for(int i = 0; i < shared_t -> Z_count; i++) {
        if(munmap(shared_t -> stops_mutex[i], sizeof(sem_t)) != 0) {
            fprintf(stderr, "ERROR: Memory unmapping failed.\n");
            exit(1);
        }
    }
    if(munmap(shared_t -> stops_mutex, sizeof(sem_t *)) != 0) {
        fprintf(stderr, "ERROR: Memory unmapping failed.\n");
        exit(1);
    }
    if(munmap(shared_t -> output_mutex, sizeof(sem_t)) != 0) {
        fprintf(stderr, "ERROR: Memory unmapping failed.\n");
        exit(1);
    }
    if(munmap(shared_t -> all_skiers_finished, sizeof(sem_t)) != 0) {
        fprintf(stderr, "ERROR: Memory unmapping failed.\n");
        exit(1);
    }
    if(munmap(shared_t -> bus_mutex, sizeof(sem_t)) != 0) {
        fprintf(stderr, "ERROR: Memory unmapping failed.\n");
        exit(1);
    }
    if(munmap(shared_t -> L_skiing, sizeof(int)) != 0) {
        fprintf(stderr, "ERROR: Memory unmapping failed.\n");
        exit(1);
    }
    if(munmap(shared_t -> stops_waiting, sizeof(int)) != 0) {
        fprintf(stderr, "ERROR: Memory unmapping failed.\n");
        exit(1);
    }
    if(munmap(shared_t -> L_boarded, sizeof(int)) != 0) {
        fprintf(stderr, "ERROR: Memory unmapping failed.\n");
        exit(1);
    }
    if(munmap(shared_t -> A, sizeof(int)) != 0) {
        fprintf(stderr, "ERROR: Memory unmapping failed.\n");
        exit(1);
    }
    if(munmap(shared_t, sizeof(shared_vars)) != 0) {
        fprintf(stderr, "ERROR: Memory unmapping failed.\n");
        exit(1);
    }
}

void custom_print(char *output, ...) {
    sem_wait(shared_t -> output_mutex); // waits for output to be done
    va_list args;
    va_start(args, output);
    fprintf(shared_t -> file, "%d: ", *(shared_t -> A));
    vfprintf(shared_t -> file, output, args);
    fflush(shared_t -> file);
    va_end(args);
    (*(shared_t -> A))++;
    sem_post(shared_t -> output_mutex); // signals that output is done
}

void bus() {
    custom_print("BUS: started\n");
    int stop = 1;
    // while all skiers aren't skiing
    while(*(shared_t -> L_skiing) != shared_t -> L_count) {
        rand_sleep(shared_t -> bus_max_time);
        // prints and increments the stop variable
        custom_print("BUS: arrived to %d\n", stop);
        for(int i = 0; (i < shared_t -> stops_waiting[stop]) && i < shared_t -> K_capacity; i++) {
            // allows the skier to board
            sem_post(shared_t -> stops_mutex[stop]);
            // increments the number of skiers that have boarded the skibus
            (*(shared_t -> L_boarded))++;
        }
        // resets the number of waiting skiers at the stop
        shared_t -> stops_waiting[stop] = 0;
        sem_wait(shared_t -> all_skiers_finished);
        custom_print("BUS: leaving %d\n", stop);
        rand_sleep(shared_t -> bus_max_time);
        // goes to next bus stop
        stop++;
        stop %= shared_t -> Z_count;
        // if the skibus is on the final stop
        if(stop == (shared_t -> Z_count) - 1) {
            custom_print("BUS: arrived to final\n");
            // all skiers unboard
            for(int i = 0; i < *(shared_t -> L_boarded); i++) {
                sem_post(shared_t -> bus_mutex);
            }
            sem_wait(shared_t -> all_skiers_finished);
            custom_print("BUS: leaving final\n");
            stop++;
            stop %= shared_t -> Z_count;
        }
    }
    custom_print("BUS: finish\n");
    exit(0);
}
// postion -> current position of skier in total
void skier(int position) {
    int order = 0; // which position the skier is in on the bus stop
    rand_sleep(shared_t -> skier_max_time);
    custom_print("L %d: started\n", position);
    rand_sleep(shared_t -> skier_max_time);
    // stop that skier will go to (index)
    int stop = rand() % (shared_t -> Z_count);
    custom_print("L %d: arrived to %d\n", position, stop);
    // increment number of waiting skiers at current stop
    shared_t -> stops_waiting[stop]++;
    // set position of skier on the bus stop
    order = shared_t -> stops_waiting[stop];
    // wait untill bus arrives
    sem_wait(shared_t -> stops_mutex[stop]);
    custom_print("L %d: boarding\n", position);
    // send signal to bus that all skiers on this stop have boarded
    if(order == shared_t -> stops_waiting[stop]) {
        sem_post(shared_t -> all_skiers_finished);
    }
    // waits until bus arrives to final bus stop
    sem_wait(shared_t -> bus_mutex);
    custom_print("L %d: going to ski\n", position);
    // sends a signal that all skiers have gone skiing
    if(order == shared_t -> stops_waiting[stop]) {
        sem_post(shared_t -> all_skiers_finished);
        (*(shared_t -> L_skiing))++;
    }
    exit(0);
}

void rand_sleep(int limit) {
    int random = rand();
    if(random > limit) {
        // to not go over the limit
        random = limit;
    }
    usleep(random);
}