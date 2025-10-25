# Compiler & flags
CC      = gcc
CFLAGS  = -Wall -Wextra -std=c99 -Isrc
LDFLAGS =
LDLIBS  =

# Directories
SRCDIR  = src
OBJDIR  = obj
DATADIR = data

# Sources / Objects
SOURCES = $(SRCDIR)/main.c \
          $(SRCDIR)/maester.c \
          $(SRCDIR)/stock.c \
          $(SRCDIR)/helper.c \
          $(SRCDIR)/trade.c

OBJECTS = $(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)
DEPS    = $(OBJECTS:.o=.d)

# Target executable
TARGET  = maester

# Default target
all: $(TARGET) $(DATADIR)

# Create data/object directories if they don't exist
$(DATADIR):
	mkdir -p $(DATADIR)

$(OBJDIR):
	mkdir -p $(OBJDIR)

# Link object files to create executable
$(TARGET): $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

# Compile source files to object files (+ header deps)
$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

# ---------------------------
# Convenience run/test targets
# ---------------------------
# NOTE: The program expects: ./maester <config.dat> <stock.db>

# Quick local run (expects data/config.dat and data/stock.db to exist)
run-local: $(TARGET) $(DATADIR)
	@if [ ! -f "$(DATADIR)/config.dat" ] || [ ! -f "$(DATADIR)/stock.db" ]; then \
		echo "Error: Missing $(DATADIR)/config.dat or $(DATADIR)/stock.db"; \
		echo "Hint: put your config/dat files under $(DATADIR)/"; \
		exit 1; \
	fi
	./$(TARGET) $(DATADIR)/config.dat $(DATADIR)/stock.db

# Test with Stark database (you must provide a matching config.dat)
test-stark: $(TARGET)
	@if [ ! -f "$(DATADIR)/config.dat" ]; then \
		echo "Error: Missing $(DATADIR)/config.dat (required as first arg)"; \
		exit 1; \
	fi
	@if [ ! -f "Statements and Files/Testing files-20251015/stark_stock.db" ]; then \
		echo "Error: stark_stock.db not found in 'Statements and Files/Testing files-20251015/'"; \
		exit 1; \
	fi
	./$(TARGET) $(DATADIR)/config.dat "Statements and Files/Testing files-20251015/stark_stock.db"

# Test with Lannister database (you must provide a matching config.dat)
test-lannister: $(TARGET)
	@if [ ! -f "$(DATADIR)/config.dat" ]; then \
		echo "Error: Missing $(DATADIR)/config.dat (required as first arg)"; \
		exit 1; \
	fi
	@if [ ! -f "Statements and Files/Testing files-20251015/lannister_stock.db" ]; then \
		echo "Error: lannister_stock.db not found in 'Statements and Files/Testing files-20251015/'"; \
		exit 1; \
	fi
	./$(TARGET) $(DATADIR)/config.dat "Statements and Files/Testing files-20251015/lannister_stock.db"

# Copy a test database to data/ for local testing (you still need a config.dat)
setup-data: $(DATADIR)
	@if [ -f "Statements and Files/Testing files-20251015/stark_stock.db" ]; then \
		cp "Statements and Files/Testing files-20251015/stark_stock.db" $(DATADIR)/stock.db; \
		echo "Copied stark_stock.db to $(DATADIR)/stock.db"; \
	else \
		echo "Warning: Test database files not found"; \
	fi

# Clean build artifacts
clean:
	rm -rf $(OBJDIR) $(TARGET)

# Clean everything including data
clean-all: clean
	rm -rf $(DATADIR)

# Include header deps if present
-include $(DEPS)

# Phony targets
.PHONY: all clean clean-all test-stark test-lannister setup-data run-local
