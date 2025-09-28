CC      = gcc
CFLAGS  = -lGL -lglfw -lwayland-client -lX11
RM      = rm -f

default: all

all: syfo

syfo: syfo.c
	$(CC) $(CFLAGS) -o syfo syfo.c
	chmod +x syfo

clean veryclean:
	$(RM) syfo
