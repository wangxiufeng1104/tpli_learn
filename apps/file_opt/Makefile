
target = main

INC_PATH += ../../inc
LIB_PATH += ../../lib
LIBS += tlpi rt

CC = gcc

SRCS += *.c

all:$(SRCS)
	$(CC) $^ -o $(target)  $(addprefix -I,$(INC_PATH)) $(addprefix -L, $(LIB_PATH)) $(addprefix -l, $(LIBS))

clean:
	rm $(target)