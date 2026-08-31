SRC=$(wildcard *.c)
OBJS=$(patsubst %.c,%.o,$(wildcard *.c))

all :  $(OBJS) oopslog
	@echo "compile done"

%.o : %.c
	$(CC) $(CFLAGS) -c $< -o $@

oopslog: 
	$(CC) $(CFLAGS) -Wall -o oopslog $(OBJS) -lguci

clean:
	rm -fr oopslog $(OBJS)
