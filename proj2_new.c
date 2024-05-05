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
void struct_init(int);
void struct_destroy();
void semaphore_init(int);
void semaphore_destroy();
void map_memory(int);
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

    struct_init(Z);
    srand(time(NULL));

    // initialize shared variables
    shared_t -> L_count = L;
    shared_t -> Z_count = Z;
    shared_t -> K_capacity = K;
    shared_t -> skier_max_time = TL;
    shared_t -> bus_max_time = TB;

    // initialize stops to 0
    for(int i = 0; i < Z; i++) {
        shared_t -> stops_waiting[i] = 0;
    }

    pid_t bus_id = fork();
    if(bus_id == -1) {
        fprintf(stderr, "ERROR: fork() failed!\n");
        struct_destroy();
        return 1;
    }
    else if(bus_id == 0) {
        bus();
    }
    for(int i = 0; i < shared_t -> L_count; i++) {
        pid_t skier_id = fork();
        if(skier_id == -1) {
            fprintf(stderr, "ERROR: fork() failed!\n");
            struct_destroy();
            return 1;
        }
        else if(skier_id == 0) {
            skier(i + 1);
        }
    }
    struct_destroy();

    return 0;
}

// **********Function definitions**********

// initializes the struct of global/shared variables
void struct_init(int Z) {
    // initializes all global variables
    map_memory(Z);
    *(shared_t -> A) = 0;
    shared_t -> L_count = 0;
    *(shared_t -> L_boarded) = 0;
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
    semaphore_init(Z);
}

void struct_destroy() {
    if(shared_t -> file != NULL) {
        fclose(shared_t -> file);
    }
    semaphore_destroy();
    unmap_memory();
}

void semaphore_init(int Z) {
    // Initialize bus_mutex semaphore
    if(sem_init(shared_t -> bus_mutex, 1, 0) == -1) {
        fprintf(stderr, "ERROR: Failed to initialize a semaphore!\n");
        struct_destroy();
        exit(1);
    }

    // Initialize all_skiers_finished semaphore
    if(sem_init(shared_t -> all_skiers_finished, 1, 0) == -1) {
        fprintf(stderr, "ERROR: Failed to initialize a semaphore!\n");
        struct_destroy();
        exit(1);
    }

    // Initialize output_mutex semaphore
    if(sem_init(shared_t -> output_mutex, 1, 1) == -1) {
        fprintf(stderr, "ERROR: Failed to initialize a semaphore!\n");
        struct_destroy();
        exit(1);
    }

    // Allocate and initialize stops_mutex array of semaphores
    for(int i = 0; i < Z; i++) {
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
        for(int i = 0; i < shared_t -> Z_count; i++) {
            if(shared_t -> stops_mutex[i] != NULL) {
                sem_destroy(shared_t -> stops_mutex[i]);
            }
        }
    }   
}

void map_memory(int Z) {
    shared_t = (shared_vars *)mmap(NULL, sizeof(shared_vars), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if(shared_t == MAP_FAILED) {
        fprintf(stderr, "ERROR: Memory mapping failed.\n");
        struct_destroy();
        exit(1);
    }
    shared_t -> A = (int *)mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if(shared_t -> A == MAP_FAILED) {
        fprintf(stderr, "ERROR: Memory mapping failed.\n");
        struct_destroy();
        exit(1);
    }
    shared_t -> L_boarded = (int *)mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if(shared_t -> L_boarded == MAP_FAILED) {
        fprintf(stderr, "ERROR: Memory mapping failed.\n");
        struct_destroy();
        exit(1);
    }
    shared_t -> L_skiing = (int *)mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if(shared_t -> L_skiing == MAP_FAILED) {
        fprintf(stderr, "ERROR: Memory mapping failed.\n");
        struct_destroy();
        exit(1);
    }
    shared_t -> stops_waiting = (int *)mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if(shared_t -> stops_waiting == MAP_FAILED) {
        fprintf(stderr, "ERROR: Memory mapping failed.\n");
        struct_destroy();
        exit(1);
    }
    shared_t -> bus_mutex = (sem_t *)mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if(shared_t -> bus_mutex == MAP_FAILED) {
        fprintf(stderr, "ERROR: Memory mapping failed.\n");
        struct_destroy();
        exit(1);
    }
    shared_t -> all_skiers_finished = (sem_t *)mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if(shared_t -> all_skiers_finished == MAP_FAILED) {
        fprintf(stderr, "ERROR: Memory mapping failed.\n");
        struct_destroy();
        exit(1);
    }
    shared_t -> output_mutex = (sem_t *)mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if(shared_t -> output_mutex == MAP_FAILED) {
        fprintf(stderr, "ERROR: Memory mapping failed.\n");
        struct_destroy();
        exit(1);
    }
    shared_t -> stops_mutex = (sem_t **)mmap(NULL, (Z * sizeof(sem_t *)), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if(shared_t -> stops_mutex == MAP_FAILED) {
        fprintf(stderr, "ERROR: Memory mapping failed.\n");
        struct_destroy();
        exit(1);
    }
    for(int i = 0; i < Z; i++) {
        shared_t -> stops_mutex[i] = (sem_t *)mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
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
    if(munmap(shared_t -> stops_mutex, (shared_t -> Z_count) * sizeof(sem_t *)) != 0) {
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
    // before posting the semaphore it segfaults somewhere
    sem_wait(shared_t -> output_mutex); // waits for output to be done
    (*(shared_t -> A))++;
    va_list args;
    va_start(args, output);
    fprintf(shared_t -> file, "%d: ", *(shared_t -> A));
    vfprintf(shared_t -> file, output, args);
    fflush(shared_t -> file);
    va_end(args);
    sem_post(shared_t -> output_mutex); // signals that output is done
}

void bus() {
    // initialize random seed
    custom_print("BUS: started\n");
    int stop = 1;
    // while all skiers aren't skiing
    while(*(shared_t -> L_skiing) != shared_t -> L_count) {
        rand_sleep(shared_t -> bus_max_time);
        // prints and increments the stop variable
        custom_print("BUS: arrived to %d\n", stop);
        // if no-one is waiting at the stop, go
        int skier_count = shared_t -> stops_waiting[(stop - 1)];
        if(skier_count == 0) {
            sem_post(shared_t -> all_skiers_finished);
        }
        for(int i = 0; (i < skier_count) && (i < shared_t -> K_capacity); i++) {
            // allows the skier to board
            sem_post(shared_t -> stops_mutex[(stop - 1)]);
            // increments the number of skiers that have boarded the skibus
            (*(shared_t -> L_boarded))++;
            (shared_t -> stops_waiting[(stop - 1)])--;
        }
        // resets the number of waiting skiers at the stop
        shared_t -> stops_waiting[(stop - 1)] = 0;
        sem_wait(shared_t -> all_skiers_finished);
        custom_print("BUS: leaving %d\n", stop);
        rand_sleep(shared_t -> bus_max_time);
        // goes to next bus stop
        stop++;
        stop %= (shared_t -> Z_count + 2);
        if(stop == 0) {
            stop++;
        }
        // if the skibus is on the final stop
        if(stop == (shared_t -> Z_count) + 1) {
            custom_print("BUS: arrived to final\n");
            // all skiers unboard
            int on_board = *(shared_t -> L_boarded);
            for(int i = 0; i < on_board; i++) {
                sem_post(shared_t -> bus_mutex);
            }
            sem_wait(shared_t -> all_skiers_finished);
            custom_print("BUS: leaving final\n");
            stop = 1;
        }
    }
    custom_print("BUS: finish\n");
    exit(0);
}
// postion -> current position of skier in total
void skier(int position) {
    // initialize random seed
    int order = 0; // which position the skier is in on the bus stop
    rand_sleep(shared_t -> skier_max_time);
    custom_print("L %d: started\n", position);
    rand_sleep(shared_t -> skier_max_time);
    // stop that skier will go to (index)
    int stop = ((rand() + getpid()) % (shared_t -> Z_count)) + 1;
    custom_print("L %d: arrived to %d\n", position, stop);
    // increment number of waiting skiers at current stop
    (shared_t -> stops_waiting[(stop - 1)])++;
    // set position of skier on the bus stop
    order = shared_t -> stops_waiting[(stop - 1)];
    // wait until bus arrives
    sem_wait(shared_t -> stops_mutex[(stop - 1)]);
    custom_print("L %d: boarding\n", position);
    order--;
    // send signal to bus that all skiers on this stop have boarded
    if(order == 0) {
        sem_post(shared_t -> all_skiers_finished);
    }
    // waits until bus arrives to final bus stop
    sem_wait(shared_t -> bus_mutex);
    custom_print("L %d: going to ski\n", position);
    (*(shared_t -> L_skiing))++;
    (*(shared_t -> L_boarded))--;
    // sends a signal that all skiers have gone skiing
    if(*(shared_t -> L_boarded) == 0) {
        sem_post(shared_t -> all_skiers_finished);
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