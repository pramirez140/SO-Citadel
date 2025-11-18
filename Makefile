CC      = gcc
CFLAGS  = -Wall -Wextra -std=c99 -Isrc
SRCDIR  = src
OBJDIR  = obj
TARGET  = maester

SOURCES = $(SRCDIR)/main.c \
          $(SRCDIR)/maester.c \
          $(SRCDIR)/network.c \
          $(SRCDIR)/missions.c \
          $(SRCDIR)/stock.c \
          $(SRCDIR)/helper.c \
          $(SRCDIR)/trade.c

OBJECTS = $(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
DEPS    = $(OBJECTS:.o=.d)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(OBJDIR):
	mkdir -p $(OBJDIR)

clean:
	rm -rf $(OBJDIR) $(TARGET)

-include $(DEPS)

.PHONY: all clean
