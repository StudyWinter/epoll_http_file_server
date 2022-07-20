src = $(wildcard ./*.c)
obj = $(patsubst ./%.c, ./%.o, $(src))

myArgs = -Wall -g -pthread
target = main
CC = gcc

ALL:$(target)

$(target):$(obj)
	$(CC) $^ -o $@ $(myArgs)

$(obj):%.o:%.c
	$(CC) -c $^ -o $@ $(myArgs)

clean:
	-rm -rf $(obj) $(target)

.PHONY: clean ALL
