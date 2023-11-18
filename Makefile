CC := clang
CFLAGS := -g -Wall -Wno-deprecated-declarations -Werror

all: tank

clean:
	rm -f tank

tank: tank.c util.c util.h scheduler.c scheduler.h
	$(CC) $(CFLAGS) -o tank tank.c util.c scheduler.c -lncurses

zip:
	@echo "Generating worm.zip file to submit to Gradescope..."
	@zip -q -r worm.zip . -x .git/\* .vscode/\* .clang-format .gitignore worm
	@echo "Done. Please upload worm.zip to Gradescope."

format:
	@echo "Reformatting source code."
	@clang-format -i --style=file $(wildcard *.c) $(wildcard *.h)
	@echo "Done."

.PHONY: all clean zip format
