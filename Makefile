all: main.o fs_api.o
	g++ -o test main.o fs_api.o

main.o: main.cpp fs_api.h
	g++ -c main.cpp fs_api.h

fs_api.o: fs_api.cpp fs_api.h
	g++ -c fs_api.cpp
