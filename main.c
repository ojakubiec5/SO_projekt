#include "common.h"

int shmid, semid, msgid;
BakeryStore *store;

void cleanup() {
    log_msg(semid, "\n[Kierownik] Zamykanie systemu i usuwanie IPC...\n");
    if (store != NULL) {
        store->shop_open = 0;
        shmdt(store);
    }
    msgctl(msgid, IPC_RMID, NULL);
    shmctl(shmid, IPC_RMID, NULL);
    semctl(semid, 0, IPC_RMID);
    kill(0, SIGTERM); 
}

void handle_sigint(int sig) { cleanup(); exit(0); }

void handle_inventory(int sig) {
    log_msg(semid, "\n!!! [Kierownik] Otrzymano sygnał INWENTARYZACJA (SIGUSR1) !!!\n");
}

void handle_evacuation(int sig) {
    log_msg(semid, "\n!!! [Kierownik] Otrzymano sygnał EWAKUACJA (SIGUSR2) !!!\n");
    if (store) store->evacuation_mode = 1;
    kill(0, SIGUSR2); 
}

int main(int argc, char *argv[]) {
    unlink(REPORT_FILE); 
    int fd = creat(REPORT_FILE, 0666); close(fd); 

    srand(time(NULL));
    signal(SIGINT, handle_sigint);
    signal(SIGUSR1, handle_inventory);
    signal(SIGUSR2, SIG_IGN); 

    key_t key_shm = ftok(FTOK_PATH, ID_SHM);
    key_t key_sem = ftok(FTOK_PATH, ID_SEM);
    key_t key_msg = ftok(FTOK_PATH, ID_MSG);

    shmid = shmget(key_shm, sizeof(BakeryStore), 0666 | IPC_CREAT);
    store = (BakeryStore*)shmat(shmid, NULL, 0);
    
    store->shop_open = 1;
    store->customers_inside = 0;
    store->cashiers_active = 0;
    store->evacuation_mode = 0;
    
    for(int i=0; i<P_TYPES; i++) {
        store->shelves[i] = 0; 
        store->shelf_capacity[i] = MAX_SHELF_CAP;
        store->price[i] = (rand() % 10) + 2;
        store->total_produced[i] = 0; 
        store->total_sold[i] = 0;
    }

    semid = semget(key_sem, SEM_COUNT, 0666 | IPC_CREAT);
    union semun arg;
    arg.val = 1; semctl(semid, SEM_SHELVES, SETVAL, arg);
    arg.val = MAX_CUSTOMERS; semctl(semid, SEM_STORE_CAP, SETVAL, arg);
    arg.val = 1; semctl(semid, SEM_STATS, SETVAL, arg);
    arg.val = 1; semctl(semid, SEM_LOG, SETVAL, arg); 

    msgid = msgget(key_msg, 0666 | IPC_CREAT);

    log_msg(semid, "[Kierownik] Godzina Tp: Uruchamiam piekarnię (Sklep zamknięty).\n");
    if (fork() == 0) { execl("./baker", "baker", NULL); exit(1); }

    log_msg(semid, "[Kierownik] Czekam na wypieki (symulacja 30min)...\n");
    sleep(3); 

    log_msg(semid, "[Kierownik] Godzina Tp+30min: Otwieram drzwi dla klientów!\n");
    if (fork() == 0) { execl("./cashier", "cashier", "1", NULL); exit(1); }
    if (fork() == 0) { execl("./cashier", "cashier", "2", NULL); exit(1); }

    log_msg(semid, "[Kierownik] Wpuszczam %d klientów...\n", NUM_TEST_CUSTOMERS);
    for (int i = 0; i < NUM_TEST_CUSTOMERS; i++) {
        if (fork() == 0) { execl("./customer", "customer", NULL); exit(0); }
        
        usleep((rand() % 200 + 100) * 1000); 
    }

    for (int i = 0; i < NUM_TEST_CUSTOMERS; i++) wait(NULL);
    
    log_msg(semid, "\n--- RAPORT KONCOWY (INWENTARYZACJA KIEROWNIKA) ---\n");
    log_msg(semid, "%-12s | Wyprodukowano | Sprzedano | Na polce\n", "Produkt");
    log_msg(semid, "-------------|---------------|-----------|---------\n");
    
    for(int i=0; i<P_TYPES; i++) {
        log_msg(semid, " %-12s| %6d        | %6d    | %6d\n", 
            PRODUCT_NAMES[i], 
            store->total_produced[i], 
            store->total_sold[i], 
            store->shelves[i]);
    }
    
    log_msg(semid, "\n[Kierownik] Zamykam sklep. Czekam na raporty pracownikow...\n");
    store->shop_open = 0; 
    
    msgctl(msgid, IPC_RMID, NULL);

    sleep(2); 
    cleanup();
    return 0;
}