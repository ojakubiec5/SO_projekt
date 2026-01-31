#include "common.h"

int main(int argc, char *argv[]) {
    if (argc < 2) exit(1);
    int cashier_id = atoi(argv[1]);
    
    int shmid = shmget(ftok(FTOK_PATH, ID_SHM), sizeof(BakeryStore), 0666);
    BakeryStore *store = (BakeryStore*)shmat(shmid, NULL, 0);
    int msgid = msgget(ftok(FTOK_PATH, ID_MSG), 0666);
    int semid = semget(ftok(FTOK_PATH, ID_SEM), SEM_COUNT, 0666);
    
    log_msg(semid, "[Kasjer %d] Otwieram kasę.\n", cashier_id);

    long shift_income = 0; 
    OrderMsg msg;

    while (store->shop_open && !store->evacuation_mode) {
        if (cashier_id == 2) {
            if (store->customers_inside < (MAX_CUSTOMERS / 2)) {
                if (msgrcv(msgid, &msg, sizeof(OrderMsg) - sizeof(long), 1, IPC_NOWAIT) == -1) {
                    if (errno == ENOMSG) { 
                        usleep(200000);
                        continue; 
                    }
                } else { 
                    goto process_order; 
                }
            }
        }
        
        if (msgrcv(msgid, &msg, sizeof(OrderMsg) - sizeof(long), 1, 0) == -1) break;

        process_order:
        if (store->evacuation_mode) break;

        char receipt_buf[512] = ""; 
        int total = 0;
        
        struct sembuf lock = {SEM_STATS, -1, 0};
        struct sembuf unlock = {SEM_STATS, 1, 0};
        
        semop(semid, &lock, 1);
        for(int i=0; i<P_TYPES; i++) {
            if (msg.cart[i] > 0) {
                int val = msg.cart[i] * store->price[i];
                total += val;
                
                store->total_sold[i] += msg.cart[i];
                
                char item_tmp[64];
                sprintf(item_tmp, "%s(x%d) ", PRODUCT_NAMES[i], msg.cart[i]);
                strcat(receipt_buf, item_tmp);
            }
        }
        semop(semid, &unlock, 1);
        
        shift_income += total;
        
        log_msg(semid, "[Kasjer %d] Klient PID: %d. Kupil: %s| Razem: %d PLN\n", 
            cashier_id, msg.customer_pid, receipt_buf, total);
            
        usleep(100000);
    }

    log_msg(semid, "=> [RAPORT Kasjer %d] Koniec zmiany. Mój utarg: %ld PLN\n", cashier_id, shift_income);

    shmdt(store);
    return 0;
}