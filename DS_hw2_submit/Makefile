APPS	:= ctree
CTREE	:= ctree

IDIR	:= ./
CC	:= gcc
CFLAGS	:= -g -I$(IDIR) -O3 -Wall -Werror=uninitialized -std=gnu99
ODIR	:= ./

_OBJ_CT	:= ctree.o stack_linkedlist.o
OBJ_CT	:= $(patsubst %,$(ODIR)/%,$(_OBJ_CT))

_OBJ_ST	:= stack_linkedlist.o
OBJ_ST	:= $(patsubst %,$(ODIR)/%,$(_OBJ_ST))

all: $(APPS)

$(ODIR)/%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(CTREE): $(OBJ_CT)
	$(CC) -o $@ $^ $(LIBS) -lpthread

clean:
	rm -f $(ODIR)/*.o $(APPS)
