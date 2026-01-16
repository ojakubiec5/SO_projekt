#include "common.h"

int main() {
    srand(getpid());
    int shmid = shmget(ftok(FTOK_PATH, ID_SHM), sizeof(BakeryStore), 0666);
    BakeryStore *store = (BakeryStore*)shmat(shmid, NULL, 0);
    int semid = semget(ftok(FTOK_PATH, ID_SEM), SEM_COUNT, 0666);

    struct sembuf lock = {SEM_SHELVES, -1, 0};
    struct sembuf unlock = {SEM_SHELVES, 1, 0};

    log_msg(semid, "[Piekarz] Zaczynam pracę.\n");

    int daily_production = 0; 

    while (store->shop_open && !store->evacuation_mode) {
        sleep(1); 

        semop(semid, &lock, 1);
        char buffer[256] = ""; 
        int any = 0;
        
        for (int i = 0; i < P_TYPES; i++) {
            int produced = rand() % 4; 
            if (store->shelves[i] + produced <= store->shelf_capacity[i]) {
                store->shelves[i] += produced;
                store->total_produced[i] += produced;
                daily_production += produced; 
                if (produced > 0) {
                    char tmp[32];
                    sprintf(tmp, "P%d(+%d) ", i, produced);
                    strcat(buffer, tmp);
                    any = 1;
                }
            }
        }
        if (any) log_msg(semid, "[Piekarz] Dostawa: %s\n", buffer);
        semop(semid, &unlock, 1);
    }

    log_msg(semid, "=> [RAPORT Piekarz] Koniec wypieków. Łącznie upiekłem: %d sztuk.\n", daily_production);

    shmdt(store);
    return 0;
}