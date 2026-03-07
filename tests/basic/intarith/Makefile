NAME = intarith

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
DQ_SRC = dq_$(NAME).dq
TB_SRC = tb_$(NAME).c
DQ_OBJ = dq_$(NAME).o
TARGET = test_$(NAME)

.PHONY: all clean run status

all: status $(TARGET)

status:
	@echo "--- Building [$(NAME)] in $(BUILD_TYPE) mode ---"

# Compile DQ source to LLVM object
$(DQ_OBJ): $(DQ_SRC)
	$(DQ) $(DQFLAGS) $(DQ_SRC) $(DQ_OBJ)

# Link C test-bed with the DQ object
$(TARGET): $(TB_SRC) $(DQ_OBJ)
	$(CC) $(CFLAGS) $(TB_SRC) $(DQ_OBJ) -o $(TARGET)

run: all
	./$(TARGET)

clean:
	rm -f $(TARGET) $(DQ_OBJ)
