all: writer reader

writer: writer.cpp SharedQueue.h
	g++ -Wall -Wextra -O3 writer.cpp -o writer

reader: reader.cpp SharedQueue.h
	g++ -Wall -Wextra -O3 reader.cpp -o reader

.PHONY: all