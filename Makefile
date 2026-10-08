CC = gcc
CFLAGS = -Wall -Wextra -Werror -std=gnu11 -Iinc -O2

SRC_DIR = src
INC_DIR = inc
OBJ_DIR = obj

SRCS = $(wildcard $(SRC_DIR)/*.c)
OBJS = $(SRCS:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
TARGET = spi_logger_sim

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

clean:
	rm -rf $(OBJ_DIR) $(TARGET)

.PHONY: all clean


-include $(OBJS:.o=.d)

test:
	$(CC) $(CFLAGS) src/logger.c src/spi_driver.c src/w25qxx.c tests/test_host.c -lpthread -o test_host
	./test_host

.PHONY: test
