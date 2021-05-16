CC=gcc
CFLAGS=-I. -std=c99
OFLAGS=-lm
TARGET=simulator
DEPS = simulator.h gll.h fileIO.h dataStructures.h tlb.h linklist.h pageTable.h

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

debug: CFLAGS += -g -O0 -D_GLIBC_DEBUG # debug flags
debug: clean $(TARGET)

$(TARGET): simulator.o gll.o fileIO.o tlb.o linklist.o pageTable.o 
	$(CC) -o $(TARGET) simulator.o gll.o fileIO.o tlb.o linklist.o pageTable.o $(OFLAGS)

.PHONY: clean

clean:
	rm -f *.o *.out $(TARGET)
