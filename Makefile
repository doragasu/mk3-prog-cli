TARGET  = mk3-prog
GLIBINC := $(shell pkg-config --cflags glib-2.0)
CFLAGS   = -O2 -Wall $(GLIBINC)
#CFLAGS ?= -g -Wall
GLIBLIB := $(shell pkg-config --libs glib-2.0)
LFLAGS   = -lmpsse -lutil $(GLIBLIB)
CC      ?= gcc
INSTALL  = install
DESTDIR ?= /usr
OBJDIR   = obj

SRCS = $(wildcard *.c)
OBJECTS := $(patsubst %.c,$(OBJDIR)/%.o,$(SRCS))

all: $(TARGET)

install: $(TARGET)
	$(INSTALL) -s -m 755 $(TARGET) $(DESTDIR)/bin/
	$(INSTALL) -m 644 mk3-prog.cfg /etc/
	$(INSTALL) -D -m 644 -t $(DESTDIR)/share/mk3-conf/ mk3prog.conf

$(TARGET): $(OBJECTS)
	$(PREFIX)$(CC) -o $(TARGET) $(OBJECTS) $(LFLAGS)

$(OBJDIR)/%.o: %.c | $(OBJDIR)
	$(PREFIX)$(CC) -c -MMD -MP $(CFLAGS) $< -o $@

$(OBJDIR):
	mkdir -p $(OBJDIR)

.PHONY: clean
clean:
	@rm -rf $(OBJDIR)

.PHONY: mrproper
mrproper: | clean
	@rm -f $(TARGET)

# Include auto-generated dependencies
-include $(SRCS:%.c=$(OBJDIR)/%.d)

