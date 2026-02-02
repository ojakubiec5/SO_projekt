Symulacja Piekarni (Producent-Konsument)

    Przedmiot: Systemy Operacyjne

    System: Linux (Ubuntu 24.04 / WSL2)

    Kompilator: gcc

    Język: C (Standard C99/C11)

    Mechanizmy: System V IPC (Semafory, Pamięć Dzielona, Kolejki Komunikatów)
Kompilacja i uruchomienie

Kompilacja

Projekt posiada plik Makefile, który automatyzuje proces kompilacji.

    make clean && make
Makefile:

    CC = gcc
    CFLAGS = -Wall -g
    DEPS = common.h
    
    all: main baker cashier customer
    
    main: main.c $(DEPS)
    	$(CC) $(CFLAGS) -o main main.c
    
    baker: baker.c $(DEPS)
    	$(CC) $(CFLAGS) -o baker baker.c
    
    cashier: cashier.c $(DEPS)
    	$(CC) $(CFLAGS) -o cashier cashier.c

    customer: customer.c $(DEPS)
    	$(CC) $(CFLAGS) -o customer customer.c
    
    clean:
    	rm -f main baker cashier customer raport.txt

Uruchomienie

Symulacją steruje proces Kierownika (main).

    ./main

Wprowadzenie

- Celem projektu było stworzenie symulacji piekarni w środowisku wielozadaniowym systemu Linux.

- Program realizuje klasyczny problem synchronizacji Producent-Konsument (Piekarz produkuje, Klient pobiera, Kasjer przetwarza).

- Architektura jest w pełni zdecentralizowana: każdy aktor (Kierownik, Piekarz, Kasjer, Klient) jest osobnym procesem systemowym.

- Synchronizacja odbywa się wyłącznie poprzez mechanizmy IPC (nie użyto wątków).

Opis kodu

main.c (Kierownik)

- Proces nadrzędny. Inicjalizuje IPC (pamięć, semafory, kolejki).

- Tworzy pracowników (Piekarz, 2x Kasjer) funkcjami fork() i exec().

- Wpuszcza klientów w pętli z losowym lub stałym opóźnieniem.

- Obsługuje sygnał SIGCHLD do sprzątania procesów zombie.

- Monitoruje zakończenie pracy (czeka na wyjście klientów i opróżnienie kolejek).

- Przeprowadza bezpieczne zamknięcie systemu (Graceful Shutdown).

baker.c (Piekarz - Producent)

- Działa w pętli nieskończonej.

- Produkuje losowe ilości towaru i umieszcza je w pamięci dzielonej (store->shelves).

- Używa semafora SEM_SHELVES do blokowania dostępu do półek podczas dostawy.

- Raportuje dostawy na ekranie i w pliku logów.


cashier.c (Kasjer - Konsument)

- Odbiera zamówienia (OrderMsg) z Kolejki Komunikatów.

- Kasjer 2 (Dynamiczny): Włącza się tylko przy dużym obciążeniu (gdy liczba klientów > 50%), w przeciwnym razie oszczędza zasoby (usleep).

- Oblicza sumę zamówienia, aktualizuje statystyki sprzedaży.

- Wypisuje paragony.

customer.c (Klient)

- Sprawdza pojemność sklepu (semafor SEM_STORE_CAP).

- Pobiera towar z półek (zmniejsza stan magazynu). Jeśli towaru brak – pomija go.

- Wysyła strukturę zamówienia do wspólnej kolejki komunikatów.

- Kończy proces (exit), co wyzwala sygnał SIGCHLD u Kierownika.

Zasoby IPC oraz elementy kluczowe
Wykorzystane mechanizmy (System V)

1. Pamięć Dzielona (shmget, shmat):

- Przechowuje strukturę BakeryStore (tablice półek, cennik, statystyki, flagi sterujące shop_open).

- Dostępna dla wszystkich procesów.

2. Semafory (semget, semctl):

- SEM_SHELVES: Mutex chroniący półki (Piekarz vs Klient).

- SEM_STORE_CAP: Semafor licznikowy (ogranicza liczbę klientów w sklepie).

- SEM_STATS: Chroni statystyki utargu kasjerów.

- SEM_LOG: Chroni plik raportu przed pomieszaniem wpisów.

3. Kolejki Komunikatów (msgget, msgsnd):

- Służą do przesyłania koszyka zakupowego od Klienta do Kasjera.

- Typ komunikatu mtype=1.

4. Sygnały

- SIGINT (Ctrl+C): Bezpieczne przerwanie symulacji i usunięcie IPC.

- SIGCHLD: Automatyczne usuwanie procesów potomnych (zapobieganie Zombie).

- SIGTERM: Zakończenie pracy pracowników przy zamykaniu sklepu.

Elementy wyróżniające i rozwiązane problemy
1. Problem "Znikających Raportów" (Race Condition)

Podczas zamykania systemu Kasjerzy nie zdążali wypisać raportu przed usunięciem semaforów przez Kierownika.

- Rozwiązanie: Zastosowano pętlę sprawdzającą życie procesów: while(kill(pid, 0) == 0). Kierownik nie usuwa zasobów, dopóki pracownicy faktycznie nie zakończą działania.

2. Obsługa tysięcy procesów (Zombie)

Przy dużej liczbie klientów tablica procesów mogła się przepełnić.

- Rozwiązanie: Zaimplementowano handler SIGCHLD z funkcją waitpid(-1, NULL, WNOHANG), który na bieżąco usuwa zakończone procesy klientów, nie blokując głównej pętli.

3. Synchronizacja przy wyjściu

Klienci wychodzili ze sklepu szybciej, niż Kasjerzy przetwarzali zamówienia.

- Rozwiązanie: Kierownik przed zamknięciem sprawdza stan kolejki komunikatów (msg_qnum) i czeka, aż spadnie do zera.

Testy:

Test 1: Standardowy przepływ i Raport Końcowy

Cel: Sprawdzenie poprawności bilansu (Produkcja - Sprzedaż = Magazyn).

    [Kierownik] Godzina Tp: Uruchamiam piekarnię.
    [Piekarz] Dostawa: Chleb(+3) Paczek(+2) Rogal(+2) Bagietka(+1)...
    [Kierownik] Wpuszczam 30 klientów...
    [Kasjer 1] Klient PID: 77477. Kupil: Paczek(x1) Ekler(x1) Sernik(x1) | Razem: 17 PLN
    ...
    --- RAPORT KONCOWY (INWENTARYZACJA KIEROWNIKA) ---
    Produkt      | Wyprodukowano | Sprzedano | Na polce
     Chleb       |      6        |      4    |      2
     Bulka       |      5        |      5    |      0
    ...
    => [RAPORT Kasjer 1] Koniec zmiany. Mój utarg: 116 PLN
    => [RAPORT Piekarz] Koniec wypieków. Łącznie upiekłem: 85 sztuk.
    [Kierownik] Czyszczenie zasobów systemu...

Wynik: ZALICZONY.

Test 2: Dynamiczny Kasjer 2

Cel: Weryfikacja, czy Kasjer 2 pomaga przy tłoku.

    [Kasjer 1] Klient PID: 75549. Kupil: Ekler(x1) Ptys(x1) Szarlotka(x1) | Razem: 18 PLN
    [Kasjer 2] Klient PID: 75550. Kupil: Chleb(x1) Rogal(x1) Bagietka(x1) | Razem: 24 PLN
    [Kasjer 1] Klient PID: 75551. Kupil: Ekler(x1) Sernik(x2) | Razem: 27 PLN
    [Kasjer 2] Klient PID: 75552. Kupil: Chleb(x1) Makowiec(x1) Precel(x1) | Razem: 24 PLN

Wynik: ZALICZONY. Kasjerzy pracują równolegle.

Test 3: Opróżnianie Kolejki (Shutdown Safety)

Cel: Sprawdzenie, czy Kierownik nie zamknie sklepu przed obsłużeniem klientów stojących w kolejce.

    [Kierownik] Klienci wyszli. Czekam na opróżnienie kolejki zamówień...
    Waiting for queue... (22 msgs left)
    [Kasjer 2] Klient PID: 77480. Kupil: Ekler(x1) | Razem: 6 PLN
    ...
    Waiting for queue... (3 msgs left)
    [Kasjer 1] Klient PID: 77505. Kupil: Drozdzowka(x1) | Razem: 3 PLN
    [Kierownik] Zamykam sklep.

Wynik: ZALICZONY. System poczekał na obsłużenie wszystkich 22 wiadomości.

Test 4: Przerwanie (SIGINT)

Cel: Sprawdzenie czyszczenia zasobów po Ctrl+C.

    ^C
    [SIGINT] Wymuszone zamknięcie...
    [Kierownik] Czyszczenie zasobów systemu...


    $ ipcs -a
    ------ Message Queues --------
    key        msqid      owner      perms      used-bytes   messages
    
    ------ Shared Memory Segments --------
    key        shmid      owner      perms      bytes      nattch     status
    
    ------ Semaphore Arrays --------
    key        semid      owner      perms      nsems

Wynik: ZALICZONY.

Linki do kodu (GitHub)

a. Tworzenie i obsługa plików

- open/write (log_msg): https://github.com/ojakubiec5/SO_projekt/blob/7e96f80a48f9aeee249392c2939972bb8fb21915/common.h#L88

b. Tworzenie procesów
- fork/exec: https://github.com/ojakubiec5/SO_projekt/blob/7e96f80a48f9aeee249392c2939972bb8fb21915/main.c#L88
- waitpid (zombie): https://github.com/ojakubiec5/SO_projekt/blob/7e96f80a48f9aeee249392c2939972bb8fb21915/main.c#L41

c. Tworzenie wątków

- Nie dotyczy (projekt wieloprocesowy).

d. Obsługa sygnałów

- signal: https://github.com/ojakubiec5/SO_projekt/blob/7e96f80a48f9aeee249392c2939972bb8fb21915/main.c#L53
- kill (check alive): https://github.com/ojakubiec5/SO_projekt/blob/7e96f80a48f9aeee249392c2939972bb8fb21915/main.c#L148

e. Synchronizacja procesów

- semop (blokada półki): https://github.com/ojakubiec5/SO_projekt/blob/7e96f80a48f9aeee249392c2939972bb8fb21915/customer.c#L48

f. Pamięć dzielona

- shmget/shmat: https://github.com/ojakubiec5/SO_projekt/blob/7e96f80a48f9aeee249392c2939972bb8fb21915/main.c#L62

g. Kolejki komunikatów

- msgsnd: https://github.com/ojakubiec5/SO_projekt/blob/7e96f80a48f9aeee249392c2939972bb8fb21915/customer.c#L58
- msgrcv: https://github.com/ojakubiec5/SO_projekt/blob/7e96f80a48f9aeee249392c2939972bb8fb21915/cashier.c#L31
- msgget: https://github.com/ojakubiec5/SO_projekt/blob/7e96f80a48f9aeee249392c2939972bb8fb21915/main.c#L85
