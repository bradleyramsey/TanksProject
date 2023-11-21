CC := clang
NCC := nvcc
CFLAGS := -g -Wall -Wno-deprecated-declarations -Werror 
NCFLAGS := -g -I/home/curtsinger/.local/include -L/home/curtsinger/.local/li
all: tank

clean:
	rm -f tank

tank: tank.c util.c util.h scheduler.c scheduler.h
	$(CC) $(CFLAGS) -o tank tank.c util.c scheduler.c -lncurses
	$(NCC) $(NCFLAGS) -o cracker cracker-gpu.cu -lcrypto -lm

format:
	@echo "Reformatting source code."
	@clang-format -i --style=file $(wildcard *.c) $(wildcard *.h)
	@echo "Done."

.PHONY: all clean zip format
