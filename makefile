CC = gcc
CFLAGS = -Wall
BASENAME = Parent

all: $(BASENAME)

$(BASENAME): $(BASENAME).o
	$(CC) $(CFLAGS) -pthread $(BASENAME).o -o $(BASENAME)

$(BASENAME).o: $(BASENAME).c
	$(CC) $(CFLAGS) -pthread -c $(BASENAME).c

run: $(BASENAME)
	./$(BASENAME) $(FLAGS)

clean:
	rm -f $(BASENAME).o

aClean: clean
	rm -f $(BASENAME)