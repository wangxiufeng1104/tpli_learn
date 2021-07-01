TARGET = sysbusd
TARGET_LIB = libsysbus.so

INC_DIRS += ../../inc \
			./lib_src \
			./src

LIB_DIRS = ./ 

LIB_DIRS += ../../lib
LIBS += tlpi rt pthread
CFLAGS  += -Wall -fPIC $(addprefix -I, $(INC_DIRS)) 
LIB_LDFLAGS += -shared -fPIC
LDFLAGS += -pthread -rdynamic $(addprefix -L, $(LIB_DIRS)) $(addprefix -l, $(LIBS))
LIB_SRC_DIRS += ./lib_src
SRC_DIRS    += ./src
CC = gcc


C_SRCS += $(foreach v,$(SRC_DIRS),$(wildcard $(v)/*.c))

LIB_SRCS += $(foreach v,$(LIB_SRC_DIRS),$(wildcard $(v)/*.c))

OBJS += $(C_OBJS) 


all: $(TARGET_LIB) $(TARGET)


$(TARGET):$(C_SRCS) $(LIB_SRCS)
	$(CC) $^   $(LDFLAGS) $(CFLAGS) -o $@

$(TARGET_LIB):$(LIB_SRCS)
	$(CC) $^ $(LIB_LDFLAGS)  $(CFLAGS) -o $@  

install:
	@echo "install so and sysbusd"

clean:
	rm $(TARGET) $(TARGET_LIB) 