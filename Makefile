BOOST_LIBS=-lboost_program_options-mt -lboost_system-mt -lboost_thread-mt -lboost_chrono-mt
CFLAGS=-O2
#CFLAGS=-g -ggdb -DTRACE

scheduler: main.cpp scheduler.cpp
	c++ -std=c++11 -I/usr/local/include -L/usr/local/lib $(CFLAGS) -o mining_simulator main.cpp scheduler.cpp $(BOOST_LIBS)
