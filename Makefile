CC = gcc
CFLAGS = -Wall -Wextra -Isrc
SRCDIR = src
OBJDIR = obj
DATADIR = data

# Source files
SOURCES = $(SRCDIR)/main.c $(SRCDIR)/stock.c $(SRCDIR)/helper.c $(SRCDIR)/maester.c
OBJECTS = $(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

# Target executable
TARGET = maester

# Default target
all: $(TARGET) $(DATADIR)

# Create data directory if it doesn't exist
$(DATADIR):
	mkdir -p $(DATADIR)

# Create object directory if it doesn't exist
$(OBJDIR):
	mkdir -p $(OBJDIR)

# Link object files to create executable
$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $^

# Compile source files to object files
$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Test with stark database
test-stark: $(TARGET)
	./$(TARGET) "Statements and Files/Testing files-20251015/stark_stock.db" dummy

# Test with lannister database
test-lannister: $(TARGET)
	./$(TARGET) "Statements and Files/Testing files-20251015/lannister_stock.db" dummy

# Copy a test database to data folder for local testing
setup-data: $(DATADIR)
	@if [ -f "Statements and Files/Testing files-20251015/stark_stock.db" ]; then \
		cp "Statements and Files/Testing files-20251015/stark_stock.db" $(DATADIR)/stock.db; \
		echo "Copied stark_stock.db to $(DATADIR)/stock.db"; \
	else \
		echo "Warning: Test database files not found"; \
	fi

# Run with local data folder
run-local: $(TARGET) $(DATADIR)
	./$(TARGET) $(DATADIR)/stock.db dummy

# Clean build artifacts
clean:
	rm -rf $(OBJDIR) $(TARGET)

# Clean everything including data
clean-all: clean
	rm -rf $(DATADIR)

# Phony targets
.PHONY: all clean clean-all test-stark test-lannister setup-data run-local
