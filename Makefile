BINDIR=$(DESTDIR)/usr/bin
CFLAGS = -Wall -Wextra -O99 -Werror -Wno-unused-parameter -Wno-missing-field-initializers

mprot: mprot.o

%.o: %.c
	$(CC) $(CFLAGS) -c $^ -o $@

.PHONY: install
install:
	install mprot $(BINDIR)/mprot

.PHONY: clean
clean:
	rm -f mprot mprot.o
