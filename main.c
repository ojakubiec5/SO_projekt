#include "common.h"
#include <sys/wait.h> 

int shmid, semid, msgid;
BakeryStore *store;

pid_t pid_baker, pid_c1, pid_c2;
volatile int finished_customers = 0; 


void cleanup() {
    log_msg(semid, "\n[Kierownik] Czyszczenie zasobów systemu...\n");

    if (store != NULL) shmdt(store);

    shmctl(shmid, IPC_RMID, NULL);
    semctl(semid, 0, IPC_RMID);
    msgctl(msgid, IPC_RMID, NULL);
    
    signal(SIGTERM, SIG_IGN); 
    kill(0, SIGTERM); 
}

void handle_sigint(int sig) { 
    printf("\n[SIGINT] Wymuszone zamknięcie...\n");
    cleanup(); 
    exit(0); 
}

void handle_inventory(int sig) {
    log_msg(semid, "\n!!! [Kierownik] Otrzymano sygnał INWENTARYZACJA (SIGUSR1) !!!\n");
}

void handle_evacuation(int sig) {
    log_msg(semid, "\n!!! [Kierownik] Otrzymano sygnał EWAKUACJA (SIGUSR2) !!!\n");
    if (store) store->evacuation_mode = 1;
    kill(0, SIGUSR2); 
}

void handle_sigchld(int sig) {
    while (waitpid(-1, NULL, WNOHANG) > 0) {
        finished_customers++;
    }
}


int main(int argc, char *argv[]) {
    unlink(REPORT_FILE); 
    int fd = creat(REPORT_FILE, 0666); close(fd); 

    srand(time(NULL));
    
    signal(SIGINT, handle_sigint);
    signal(SIGUSR1, handle_inventory);
    signal(SIGUSR2, SIG_IGN); 
    signal(SIGCHLD, handle_sigchld); 

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

    log_msg(semid, "[Kierownik] Godzina Tp: Uruchamiam piekarnię.\n");
    if ((pid_baker = fork()) == 0) { execl("./baker", "baker", NULL); exit(1); }

    log_msg(semid, "[Kierownik] Czekam na wypieki (symulacja 30min)...\n");
    sleep(3); 

    log_msg(semid, "[Kierownik] Otwieram drzwi!\n");
    if ((pid_c1 = fork()) == 0) { execl("./cashier", "cashier", "1", NULL); exit(1); }
    if ((pid_c2 = fork()) == 0) { execl("./cashier", "cashier", "2", NULL); exit(1); }

    log_msg(semid, "[Kierownik] Wpuszczam %d klientów...\n", NUM_TEST_CUSTOMERS);
    
    for (int i = 0; i < NUM_TEST_CUSTOMERS; i++) {
        if (fork() == 0) { 
            execl("./customer", "customer", NULL); 
            perror("Exec error"); exit(1); 
        }
        usleep(2000); 
    }

    while (finished_customers < NUM_TEST_CUSTOMERS) {
        usleep(100000); 
    }
    usleep(200000);

    struct msqid_ds buf;
    log_msg(semid, "[Kierownik] Klienci wyszli. Czekam na opróżnienie kolejki zamówień...\n");
    
    int queue_waits = 0;
    do {
        msgctl(msgid, IPC_STAT, &buf); 
        if (buf.msg_qnum > 0) {
            if (queue_waits % 10 == 0) 
                log_msg(semid, "Waiting for queue... (%ld msgs left)\n", buf.msg_qnum);
            usleep(100000); 
            queue_waits++;
        }
    } while (buf.msg_qnum > 0 && queue_waits < 100); 

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
    
    log_msg(semid, "\n[Kierownik] Zamykam sklep.\n");
    store->shop_open = 0; 
    msgctl(msgid, IPC_RMID, NULL); 

    log_msg(semid, "[Kierownik] Czekam na wyjście pracowników...\n");
    
    int active_workers = 3;
    int timeout = 0;
    while (active_workers > 0 && timeout < 50) {
        active_workers = 0;
        if (kill(pid_baker, 0) == 0) active_workers++;
        if (kill(pid_c1, 0) == 0) active_workers++;
        if (kill(pid_c2, 0) == 0) active_workers++;
        
        if (active_workers > 0) {
            usleep(100000);
            timeout++;
        }
    }
    
    cleanup();
    return 0;
}