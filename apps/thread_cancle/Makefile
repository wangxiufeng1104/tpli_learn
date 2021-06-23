
target = pthread_cancle

INC_PATH += -I../../inc
LIB_PATH += -L../../lib
LIBS += -ltlpi -lpthread

CC = gcc

SRCS += *.c

all:$(SRCS)
	$(CC) $^ -o $(target) $(INC_PATH) $(LIB_PATH) $(LIBS)

clean:
	rm $(target)