CC = gcc
CFLAGS = -Wall -g -D_GNU_SOURCE
LIBS = 

TARGETS = main baker cashier customer

all: $(TARGETS)

main: main.c common.h
	$(CC) $(CFLAGS) -o main main.c $(LIBS)

baker: baker.c common.h
	$(CC) $(CFLAGS) -o baker baker.c $(LIBS)

cashier: cashier.c common.h
	$(CC) $(CFLAGS) -o cashier cashier.c $(LIBS)

customer: customer.c common.h
	$(CC) $(CFLAGS) -o customer customer.c $(LIBS)

clean:
	rm -f $(TARGETS) *.o
