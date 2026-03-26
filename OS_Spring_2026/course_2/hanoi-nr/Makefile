all: hanoi-r hanoi-nr hanoi-forloop

hanoi-r: hanoi-main.c hanoi-r.c
	gcc -g -o $@ $^

hanoi-nr: hanoi-main.c hanoi-nr.c
	gcc -g -o $@ $^

hanoi-forloop: hanoi_forloop.c
	gcc -g -o $@ $^

clean:
	rm -f hanoi-r hanoi-nr hanoi-forloop
