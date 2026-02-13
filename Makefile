cc := clang
cflags := -std=c11 -Wall -Wextra -pedantic -MMD -MP
incdir := -Ihttp
objdir := build/obj
testdir := build/tests

MODE ?= debug
ifeq ($(MODE), release)
	cflags += -O2 -ffast-math -march=native
else
	cflags += -g3 -fsanitize=address,undefined,leak
endif

srcs := $(wildcard src/*.c)
objs := $(srcs:src/%.c=$(objdir)/%.o)
deps := $(srcs:src/%.c=$(objdir)/%.d)
exec := build/http2
-include $(deps)

testfiles := $(wildcard test/*.c)
tests := $(testfiles:test/%.c=$(testdir)/%)

.PHONY: all test run clean

all: $(exec)

run: $(exec)
	@./$(exec)

$(exec): $(objs)
	$(cc) $(cflags) $^ -o $@

$(objdir)/%.o: src/%.c | $(objdir)
	$(cc) $(cflags) $(incdir) -c $< -o $@

$(testdir)/%: tests/%.c | $(testdir)
	$(cc) $(cflags) $(incdir) $^ -o $@ -lcriterion

test: $(tests)
	@for t in $(tests); do \
		echo "[TEST]: $$t"; \
		./$$t; \
		echo ""; \
	done

$(objdir):
	mkdir -p $@

clean:
	rm -rf $(exec) $(objdir)
