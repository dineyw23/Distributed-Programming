goldChase: Signal.cpp libmap.a goldchase.h Signal.h
	g++ -g -std=c++0x Signal.cpp -o goldChase -L. -L /lib64 -lmap -lpanel -lncurses -lrt -l pthread

libmap.a: Signal.o Screen.o Map.o
	ar -r libmap.a Signal.o Screen.o Map.o

Signal.o: Signal.cpp 
	g++ -c -std=c++0x Signal.cpp 

Screen.o: Screen.cpp
	g++ -c Screen.cpp

Map.o: Map.cpp
	g++ -c Map.cpp

clean:
	rm -f Signal.o Screen.o Map.o libmap.a goldChase 
