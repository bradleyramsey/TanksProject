CC := g++
NCC := nvcc
CFLAGS := -g -Wall -Wno-deprecated-declarations -Werror -L/home/goodellb/csc213/final
NCFLAGS := -g -I/home/curtsinger/.local/include -L/home/curtsinger/.local/li 

all: main.cpp tank.cpp cracker-gpu.cu cracker-gpu.h util.cpp util.h scheduler.cpp scheduler.h
	$(NCC) $(NCFLAGS) -o main main.cpp tank.cpp util.cpp scheduler.cpp message.cpp cracker-gpu.cu -lncurses -lpthread

clean:
	rm -f tank

tank: tank.c util.c util.h scheduler.c scheduler.h
	$(CC) $(CFLAGS) -o tank tank.c util.c scheduler.c -lncurses

format:
	@echo "Reformatting source code."
	@clang-format -i --style=file $(wildcard *.c) $(wildcard *.h)
	@echo "Done."

.PHONY: all clean zip format
