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

Test 1: Symulacja Deficytu Towaru

Cel: Weryfikacja logiki synchronizacji w sytuacji, gdy popyt drastycznie przewyższa podaż. Test sprawdza, czy klienci poprawnie obsługują puste półki (nie kupują "ujemnych" towarów) i nie blokują dostępu do sklepu.
        [Kierownik] Godzina Tp: Uruchamiam piekarnię.
        [Kierownik] Czekam na wypieki (symulacja 30min)...
        [Piekarz] Zaczynam pracę.
        [Piekarz] Dostawa: Chleb(+3) Bulka(+3) Paczek(+1) Rogal(+2) Bagietka(+1) Ekler(+1) Ptys(+3) Sernik(+1) Makowiec(+1) Szarlotka(+3) Precel(+1)
        [Piekarz] Dostawa: Bulka(+1) Drozdzowka(+1) Bagietka(+1) Ekler(+2) Precel(+2)
        [Kierownik] Otwieram drzwi!
        [Kierownik] Wpuszczam 100 klientów...
        [Kasjer 1] Otwieram kasę.
        [Kasjer 2] Otwieram kasę.
        [Piekarz] Dostawa: Chleb(+1) Drozdzowka(+1) Ekler(+2) Sernik(+3) Makowiec(+3) Precel(+1)
        [Kasjer 1] Klient PID: 566959. Kupil: Chleb(x1) Bagietka(x1) | Razem: 11 PLN
        [Kasjer 2] Klient PID: 566960. Kupil: Drozdzowka(x1) | Razem: 7 PLN
        [Kasjer 1] Klient PID: 566961. Kupil: Bulka(x1) Szarlotka(x2) | Razem: 22 PLN
        [Kierownik] Klienci wyszli. Czekam na opróżnienie kolejki zamówień...
        Waiting for queue... (22 msgs left)
        [Kasjer 2] Klient PID: 566962. Kupil: Ekler(x1) | Razem: 7 PLN
        [Kasjer 1] Klient PID: 566963. Kupil: Chleb(x1) Bulka(x1) Makowiec(x1) | Razem: 16 PLN
        [Kasjer 2] Klient PID: 566964. Kupil: Szarlotka(x1) | Razem: 8 PLN
        [Kasjer 1] Klient PID: 566965. Kupil: Chleb(x1) Sernik(x2) Makowiec(x1) | Razem: 30 PLN
        [Kasjer 2] Klient PID: 566966. Kupil: Chleb(x1) | Razem: 8 PLN
        [Kasjer 1] Klient PID: 566967. Kupil: Ekler(x1) | Razem: 7 PLN
        [Piekarz] Dostawa: Chleb(+1) Bulka(+1) Drozdzowka(+2) Rogal(+2) Bagietka(+3) Ekler(+3) Ptys(+2) Sernik(+1) Szarlotka(+3) Precel(+1)
        [Kasjer 2] Klient PID: 566968. Kupil: Drozdzowka(x1) Precel(x1) | Razem: 9 PLN
        [Kasjer 1] Klient PID: 566969. Kupil: Rogal(x1) Makowiec(x1) | Razem: 10 PLN
        [Kasjer 2] Klient PID: 566971. Kupil: Paczek(x1) Sernik(x2) | Razem: 31 PLN
        [Kasjer 1] Klient PID: 566972. Kupil: Bagietka(x1) | Razem: 3 PLN
        Waiting for queue... (12 msgs left)
        [Kasjer 2] Klient PID: 566973. Kupil: Precel(x1) | Razem: 2 PLN
        [Kasjer 1] Klient PID: 566974. Kupil: Ptys(x1) | Razem: 7 PLN
        [Kasjer 2] Klient PID: 566975. Kupil: Rogal(x1) Ptys(x1) | Razem: 15 PLN
        [Kasjer 1] Klient PID: 566976. Kupil: Bulka(x1) | Razem: 6 PLN
        [Kasjer 2] Klient PID: 566977. Kupil: Makowiec(x1) | Razem: 2 PLN
        [Kasjer 1] Klient PID: 566979. Kupil: Bulka(x1) | Razem: 6 PLN
        [Piekarz] Dostawa: Chleb(+1) Bulka(+1) Drozdzowka(+1) Rogal(+2) Ptys(+3) Sernik(+2) Szarlotka(+1) Precel(+2)
        [Kasjer 2] Klient PID: 566980. Kupil: Precel(x1) | Razem: 2 PLN
        [Kasjer 1] Klient PID: 566982. Kupil: Ptys(x1) | Razem: 7 PLN
        [Kasjer 2] Klient PID: 566984. Kupil: Ekler(x1) | Razem: 7 PLN
        [Kasjer 1] Klient PID: 566985. Kupil: Precel(x1) | Razem: 2 PLN
        Waiting for queue... (2 msgs left)
        [Kasjer 1] Klient PID: 566990. Kupil: Ekler(x1) | Razem: 7 PLN
        [Kasjer 2] Klient PID: 566998. Kupil: Ekler(x1) | Razem: 7 PLN
        
        --- RAPORT KONCOWY (INWENTARYZACJA KIEROWNIKA) ---
        Produkt      | Wyprodukowano | Sprzedano | Na polce
        -------------|---------------|-----------|---------
         Chleb       |      6        |      4    |      2
         Bulka       |      6        |      4    |      2
         Paczek      |      1        |      1    |      0
         Drozdzowka  |      5        |      2    |      3
         Rogal       |      6        |      2    |      4
         Bagietka    |      5        |      2    |      3
         Ekler       |      8        |      5    |      3
         Ptys        |      8        |      3    |      5
         Sernik      |      7        |      4    |      3
         Makowiec    |      4        |      4    |      0
         Szarlotka   |      7        |      3    |      4
         Precel      |      7        |      4    |      3
        
        [Kierownik] Zamykam sklep.
        [Kierownik] Czekam na wyjście pracowników...
        => [RAPORT Kasjer 1] Koniec zmiany. Mój utarg: 134 PLN
        => [RAPORT Kasjer 2] Koniec zmiany. Mój utarg: 105 PLN
        [Piekarz] Dostawa: Bulka(+3) Paczek(+2) Bagietka(+2) Precel(+2)
        => [RAPORT Piekarz] Koniec wypieków. Łącznie upiekłem: 79 sztuk.

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

Test 4: Stress test (symulacja bez sleepów)

Cel: Weryfikacja poprawności działania semaforów oraz stabilności systemu operacyjnego przy próbie utworzenia ogromnej liczby procesów w bardzo krótkim czasie. Test sprawdza, czy program jest odporny na wyczerpanie limitu procesów

        (...)
        Piekarz] Dostawa: Szarlotka(+1)
        [Kasjer 1] Klient PID: 678128. Kupil: Sernik(x1) Makowiec(x1) Precel(x1) | Razem: 19 PLN
        [Kasjer 2] Klient PID: 678130. Kupil: Drozdzowka(x1) | Razem: 3 PLN
        [Kasjer 1] Klient PID: 678131. Kupil: Bulka(x1) Drozdzowka(x1) | Razem: 12 PLN
        [Piekarz] Dostawa: Drozdzowka(+1)
        [Piekarz] Dostawa: Bulka(+3)
        [Piekarz] Dostawa: Bulka(+1) Drozdzowka(+1) Makowiec(+2) Szarlotka(+1)
        [Kasjer 2] Klient PID: 678132. Kupil: Bulka(x3) Makowiec(x1) | Razem: 33 PLN
        [Kasjer 1] Klient PID: 678133. Kupil: Bulka(x1) Makowiec(x1) Szarlotka(x1) | Razem: 19 PLN
        [Piekarz] Dostawa: Bulka(+1) Sernik(+1) Makowiec(+1)
        [Kierownik] Klienci wyszli. Czekam na opróżnienie kolejki zamówień...
        
        --- RAPORT KONCOWY (INWENTARYZACJA KIEROWNIKA) ---
        Produkt      | Wyprodukowano | Sprzedano | Na polce
        -------------|---------------|-----------|---------
         Chleb       |   1305        |   1105    |    200
         Bulka       |   1238        |   1038    |    200
         Paczek      |   1248        |   1048    |    200
         Drozdzowka  |   1257        |   1057    |    200
         Rogal       |   1205        |   1005    |    200
         Bagietka    |   1233        |   1033    |    200
         Ekler       |   1205        |   1005    |    200
         Ptys        |   1246        |   1046    |    200
         Sernik      |   1270        |   1070    |    200
         Makowiec    |   1259        |   1059    |    200
         Szarlotka   |   1235        |   1035    |    200
         Precel      |   1228        |   1028    |    200
        
        [Kierownik] Zamykam sklep.
        => [RAPORT Kasjer 2] Koniec zmiany. Mój utarg: 23837 PLN
        [Kierownik] Czekam na wyjście pracowników...
        => [RAPORT Piekarz] Koniec wypieków. Łącznie upiekłem: 14929 sztuk.
        => [RAPORT Kasjer 1] Koniec zmiany. Mój utarg: 52239 PLN

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
