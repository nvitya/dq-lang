NAME = dq_hello

# Tools
CC = gcc
DQ = dq-comp

# --- Default to Debug Mode ---
CFLAGS = -g -O0
DQFLAGS = -g
BUILD_TYPE = DEBUG

# Switch to Release if requested (e.g., 'make RELEASE=1')
ifeq ($(RELEASE), 1)
    CFLAGS = -O2
    DQFLAGS = -O2
    BUILD_TYPE = RELEASE
endif

# Derived Filenames
DQ_SRC = $(NAME).dq
DQ_OBJ = $(NAME).o
TARGET = $(NAME)

.PHONY: all clean run status

all: status $(TARGET)

status:
	@echo "--- Building [$(NAME)] in $(BUILD_TYPE) mode ---"

# Compile DQ source to object
$(DQ_OBJ): $(DQ_SRC)
	$(DQ) $(DQFLAGS) -c $(DQ_SRC) -o $(DQ_OBJ)

# Link DQ object directly with libc (no C testbed)
$(TARGET): $(DQ_OBJ)
	$(CC) $(CFLAGS) $(DQ_OBJ) -o $(TARGET)

run: all
	./$(TARGET)

clean:
	rm -f $(TARGET) $(DQ_OBJ)
