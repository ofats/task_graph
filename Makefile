CC=clang++
CFLAGS=-std=c++17 -DLOCALHOST -O3 -pthread

exec: main
	./$^
main: main.cpp
	$(CC) -o $@ $(CFLAGS) $^
clean:
	rm main
