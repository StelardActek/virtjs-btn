CFLAGS += -Wall -Wextra -Wpedantic -Werror
CFLAGS += $(shell pkgconf --cflags libevdev)
LDLIBS += $(shell pkgconf --libs libevdev)

PROJECT := virtjs-btn

DIR_SRC += .
DIR_SRC += ./src

SRC_C += $(wildcard $(addsuffix /*.c, $(DIR_SRC)))
OBJ := $(patsubst %.c, %.o, $(SRC_C))
EXE := $(PROJECT)

CC_PREFIX := 
CC := $(CC_PREFIX)gcc

.PHONY:all

all:$(OBJ) $(EXE)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@ 

$(EXE): $(OBJ)
	$(CC) $(CFLAGS) $(LDLIBS) $(OBJ) -o $@ 

clean:
	rm -r $(EXE) $(OBJ)
