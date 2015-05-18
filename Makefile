BOOST_LIBS=-lboost_system-mt -lboost_thread-mt -lboost_chrono-mt

scheduler: main.cpp scheduler.cpp
	clang++ -std=c++11 -I/usr/local/include -L/usr/local/lib -g -ggdb -o mining_simulator main.cpp scheduler.cpp $(BOOST_LIBS)
