NAME = dq_typedef

DQ = dq-comp

# --- Default to Debug Mode ---
DQFLAGS = -g
BUILD_TYPE = DEBUG

# Switch to Release if requested (e.g., 'make RELEASE=1')
ifeq ($(RELEASE), 1)
    DQFLAGS = -O2
    BUILD_TYPE = RELEASE
endif

# Derived Filenames
DQ_SRC = $(NAME).dq
TARGET = $(NAME)

.PHONY: all clean run status

all: status $(TARGET)

status:
	@echo "--- Building [$(NAME)] in $(BUILD_TYPE) mode ---"

$(TARGET): $(DQ_SRC)
	$(DQ) $(DQFLAGS) $(DQ_SRC) -o $(TARGET)

run: all
	./$(TARGET)

clean:
	rm -f $(TARGET) $(NAME).o
