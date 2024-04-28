#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/types.h>

// Global variables in a struct
struct shared_vars {
    FILE *file; // the output file
    int A; // number of lines in the output file/actions
    int L_waiting; // number of skiers waiting
    int L_boarded; // number of skiers onboard
    int Z_count; // number of stops
    int K_capacity; // bus capacity
    sem_t bus_mutex; // mutex for the skibus
    sem_t L_mutex; // mutex for skiers
    sem_t 
};

// Function prototypes
void custom_print(FILE *, char *, ...);


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
    // check if the argument is valid \
    endptr should be a null terminator ('\0') after a successful conversion
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
    // check if the argument is valid \
    endptr should be a null terminator ('\0') after a successful conversion
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
    // check if the argument is valid \
    endptr should be a null terminator ('\0') after a successful conversion
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
    // check if the argument is valid \
    endptr should be a null terminator ('\0') after a successful conversion
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
    // check if the argument is valid \
    endptr should be a null terminator ('\0') after a successful conversion
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
    FILE *file = fopen("proj2.out", "w");
    if(file == NULL) {
        fprintf(stderr, "ERROR: File failed to open\n");
    }
    // set buffer to NULL to avoid buffering
    setbuf(file, NULL);

    // initializing a semaphore list
    sem_t **mutex = NULL;
    if(sem_init(mutex, 1, 1) == -1) {
        fprintf(stderr, "ERROR: Unable to initialize semaphore.\n");
        return 1;
    }

    return 0;
}

void custom_print(FILE *file, char *output, ...) {
    va_list args;
    va_start(args, output);
    vfprintf(stdout, output, args);
    fflush(file);
}