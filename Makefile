all: writer reader

writer: writer.cpp SharedQueue.h
	g++ -Wall writer.cpp -o writer

reader: reader.cpp SharedQueue.h
	g++ -Wall reader.cpp -o reader

.PHONY: all