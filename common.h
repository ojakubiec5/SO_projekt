#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <sys/wait.h>
#include <fcntl.h>  
#include <string.h> 
#include <stdarg.h> 

#define P_TYPES 12         
#define MAX_SHELF_CAP 20   
#define MAX_CUSTOMERS 10   
#define NUM_TEST_CUSTOMERS 50 
#define K_FACTOR (MAX_CUSTOMERS / 2) 
#define REPORT_FILE "raport.txt"

#define FTOK_PATH "." 
#define ID_SHM 10
#define ID_SEM 11
#define ID_MSG 12

static const char *PRODUCT_NAMES[P_TYPES] = {
    "Chleb",      "Bulka",     "Paczek",    "Drozdzowka",
    "Rogal",      "Bagietka",  "Ekler",     "Ptys",
    "Sernik",     "Makowiec",  "Szarlotka", "Precel"
};

typedef struct {
    int shelves[P_TYPES];      
    int shelf_capacity[P_TYPES]; 
    int price[P_TYPES];        
    
    int customers_inside;      
    int shop_open;             
    int cashiers_active;
    int evacuation_mode;
    
    int total_produced[P_TYPES];
    int total_sold[P_TYPES];
} BakeryStore;

#define SEM_SHELVES 0    
#define SEM_STORE_CAP 1  
#define SEM_STATS 2      
#define SEM_LOG 3        
#define SEM_COUNT 4      

typedef struct {
    long mtype;             
    int customer_pid;       
    int cart[P_TYPES];      
} OrderMsg;

#define CHECK(x, msg) \
    if ((x) == -1) { perror(msg); exit(EXIT_FAILURE); }

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

static void log_msg(int semid, const char *format, ...) {
    char buffer[512];
    va_list args;
    
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    printf("%s", buffer);

    struct sembuf lock = {SEM_LOG, -1, 0};
    struct sembuf unlock = {SEM_LOG, 1, 0};
    
    if (semid != -1) semop(semid, &lock, 1);

    int fd = open(REPORT_FILE, O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (fd != -1) {
        write(fd, buffer, strlen(buffer));
        close(fd);
    }

    if (semid != -1) semop(semid, &unlock, 1);
}

#endif