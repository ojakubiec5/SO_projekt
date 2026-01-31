#include "common.h"

int semid;
BakeryStore *store;

void handle_evacuation(int sig) {
    exit(0);
}

int main() {
    signal(SIGUSR2, handle_evacuation);
    srand(getpid() ^ time(NULL));
    
    int shmid = shmget(ftok(FTOK_PATH, ID_SHM), sizeof(BakeryStore), 0666);
    if (shmid == -1) { perror("Customer shmget"); exit(1); }

    store = (BakeryStore*)shmat(shmid, NULL, 0);
    semid = semget(ftok(FTOK_PATH, ID_SEM), SEM_COUNT, 0666);
    int msgid = msgget(ftok(FTOK_PATH, ID_MSG), 0666);
    if (msgid == -1) { perror("Customer msgget"); exit(1); }

    struct sembuf enter = {SEM_STORE_CAP, -1, 0};
    struct sembuf leave = {SEM_STORE_CAP, 1, 0};
    struct sembuf lock_shelf = {SEM_SHELVES, -1, 0};
    struct sembuf unlock_shelf = {SEM_SHELVES, 1, 0};

    if (semop(semid, &enter, 1) == -1) exit(1);
    
    struct sembuf lock_stats = {SEM_STATS, -1, 0};
    struct sembuf unlock_stats = {SEM_STATS, 1, 0};
    semop(semid, &lock_stats, 1);
    store->customers_inside++;
    semop(semid, &unlock_stats, 1);

    OrderMsg order;
    order.mtype = 1; 
    order.customer_pid = getpid();
    for(int i=0; i<P_TYPES; i++) order.cart[i] = 0;

    int items_count = 0;
    int items_to_buy = (rand() % 4) + 1;

    for(int i=0; i < items_to_buy; i++) {
        if (store->evacuation_mode) break;
        int prod = rand() % P_TYPES;
        int quantity = 1;

        semop(semid, &lock_shelf, 1);
        if (store->shelves[prod] >= quantity) {
            store->shelves[prod] -= quantity;
            order.cart[prod] += quantity;
            items_count++;
        }
        semop(semid, &unlock_shelf, 1);
    }

    if (items_count > 0 && !store->evacuation_mode) {
        if (msgsnd(msgid, &order, sizeof(OrderMsg) - sizeof(long), 0) == -1) {
            perror("!!! BLAD msgsnd w Customer !!!"); // To nam powie prawdę w logach
        }
    }

    semop(semid, &lock_stats, 1);
    store->customers_inside--;
    semop(semid, &unlock_stats, 1);
    
    semop(semid, &leave, 1);
    shmdt(store);
    return 0;
}