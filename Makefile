.PHONY: all

NAME=mbw
TARFILE=${NAME}.tar.gz
CC=gcc

CF=
OPT=-O3 -Ofast 
DEFS=-D__AVX256__

.s.o:
	$(CC) -c -o $@ $<

.c.o:
	$(CC) $(CF) $(OPT) $(DEFS) -c -o $@ $<

$(NAME): mbw.o memavail.o mc32nt4p_s.o
	$(CC) -o $@ $^

clean:
	-rm -f *.o
	-rm -f ${NAME}
	-rm -f ${NAME}.tar.gz

${TARFILE}: clean
	 tar cCzf .. ${NAME}.tar.gz --exclude-vcs ${NAME} || true

rpm: ${TARFILE}
	 rpmbuild -ta ${NAME}.tar.gz 
