cc := clang
cflags := -std=c11 -Wall -Wextra -pedantic -MMD -MP -D_GNU_SOURCE
incdir := -Itcp -Iutils
objdir := build/obj

MODE ?= debug
ifeq ($(MODE), release)
	cflags += -O2 -ffast-math -march=native
else
	cflags += -g3 -fsanitize=address,undefined,leak
endif

srcs := $(wildcard src/*.c)
objs := $(srcs:src/%.c=$(objdir)/%.o)
deps := $(srcs:src/%.c=$(objdir)/%.d)
exec := build/tcp
-include $(deps)

.PHONY: all test run clean

all: $(exec)

run: $(exec)
	@./$(exec)

$(exec): $(objs)
	$(cc) $(cflags) $^ -o $@

$(objdir)/%.o: src/%.c | $(objdir)
	$(cc) $(cflags) $(incdir) -c $< -o $@

$(objdir):
	mkdir -p $@

test:
	@python3 ./test/test_tcp.py

clean:
	rm -rf $(exec) $(objdir)
