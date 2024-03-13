#CFLAGS=-O2 -Wall -g
NAME=mbw
TARFILE=${NAME}.tar.gz

.s.o:
	$(CC) -c -o $@ $<

.c.o:
	$(CC) -c -O3 -Ofast -D__AVX128__ -D__AVX256__ -o $@ $<

mbw: mbw.o mc32nt4p_s.o
	$(CC) -o $@ $^

clean:
	rm -f mbw
	rm -f ${NAME}.tar.gz

${TARFILE}: clean
	 tar cCzf .. ${NAME}.tar.gz --exclude-vcs ${NAME} || true

rpm: ${TARFILE}
	 rpmbuild -ta ${NAME}.tar.gz 
