BINDIR=$(DESTDIR)/usr/bin
CFLAGS = -g -Wall -Wextra -O99 -Werror -Wno-unused-parameter -Wno-missing-field-initializers -Wno-discarded-qualifiers
LDFLAGS = -lrt

mprot: mprot.o

%.o: %.c
	$(CC) $(CFLAGS) -c $^ -o $@

.PHONY: install
install:
	install mprot $(BINDIR)/mprot

.PHONY: clean
clean:
	rm -f mprot mprot.o
