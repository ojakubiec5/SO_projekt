#include "common.h"

int semid;
BakeryStore *store;

void handle_evacuation(int sig) {
    log_msg(semid, "!!! [Klient %d] EWAKUACJA! Uciekam! !!!\n", getpid());
    if (store) store->customers_inside--; 
    exit(0);
}

int main() {
    signal(SIGUSR2, handle_evacuation);
    srand(getpid() ^ time(NULL));
    
    int shmid = shmget(ftok(FTOK_PATH, ID_SHM), sizeof(BakeryStore), 0666);
    store = (BakeryStore*)shmat(shmid, NULL, 0);
    semid = semget(ftok(FTOK_PATH, ID_SEM), SEM_COUNT, 0666);
    int msgid = msgget(ftok(FTOK_PATH, ID_MSG), 0666);

    struct sembuf enter = {SEM_STORE_CAP, -1, 0};
    struct sembuf leave = {SEM_STORE_CAP, 1, 0};
    struct sembuf lock_shelf = {SEM_SHELVES, -1, 0};
    struct sembuf unlock_shelf = {SEM_SHELVES, 1, 0};
    struct sembuf lock_stats = {SEM_STATS, -1, 0};
    struct sembuf unlock_stats = {SEM_STATS, 1, 0};

    if (store->evacuation_mode) exit(0);
    
    semop(semid, &enter, 1);
    
    semop(semid, &lock_stats, 1);
    store->customers_inside++;
    semop(semid, &unlock_stats, 1);

    OrderMsg order;
    order.mtype = 1; 
    order.customer_pid = getpid();
    for(int i=0; i<P_TYPES; i++) order.cart[i] = 0;

    int items = (rand() % 4) + 2;
    for(int i=0; i < items; i++) {
        if (store->evacuation_mode) raise(SIGUSR2);
        int prod = rand() % P_TYPES;
        int quantity = (rand() % 2) + 1;

        semop(semid, &lock_shelf, 1);
        if (store->shelves[prod] >= quantity) {
            store->shelves[prod] -= quantity;
            order.cart[prod] += quantity;
        }
        semop(semid, &unlock_shelf, 1);
    }

    if (store->evacuation_mode) raise(SIGUSR2);
    msgsnd(msgid, &order, sizeof(OrderMsg) - sizeof(long), 0);
    
    semop(semid, &lock_stats, 1);
    store->customers_inside--;
    semop(semid, &unlock_stats, 1);
    
    semop(semid, &leave, 1);
    shmdt(store);
    return 0;
}