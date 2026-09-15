obj_pub = cluster.o io.o log.o lookup.o mount.o node.o time.o utf.o utils.o

SOURCES = $(wildcard *.c)
OBJS = $(patsubst %.c,%.o,$(SOURCES))

all: $(OBJS) libraries fsck.exfat mkfs.exfat

%.o: %.c
	$(CC) $(CFLAGS) -fPIC -c -o $@ $<

fsck.exfat: fsck/main.o
	$(CC) $^ -o $@ $(LDFLAGS) -L. -lexfat

mkfs.exfat: mkfs/cbm.o mkfs/fat.o mkfs/main.o mkfs/mkexfat.o mkfs/rootdir.o mkfs/uct.o mkfs/uctc.o mkfs/vbr.o
	$(CC) $^ -o $@ $(LDFLAGS) -L. -lexfat

libraries: 
	$(CC) -shared -fPIC -o libexfat.so $(obj_pub) -ldl

clean:
	rm -f *.o
