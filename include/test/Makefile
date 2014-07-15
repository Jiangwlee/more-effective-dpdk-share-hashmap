CC = g++
FLAGS = -DDEBUG
TARGETDIR = build
INCLUDE = -I../

vpath %.h ../

test : main.o
	$(CC) -o test main.o

main.o : main.cpp
	$(CC) $(FLAGS) $(INCLUDE) -c main.cpp

clean : 
	rm -f *.o test
