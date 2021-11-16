LDFLAGS =
#CFLAGS  = -Ofast -march=native -Wall -pedantic -Werror -lm
CFLAGS  = -ggdb -march=native -Wall -pedantic -Werror -lm
CC      = cc
EXE     = exp

all: $(EXE)

$(EXE): main.c
	$(CC) -o $@ $^ $(LDFLAGS) $(CFLAGS)

.PHONY: clean

clean:
	rm -f $(EXE)
