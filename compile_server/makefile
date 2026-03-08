compile_server:compile_server.cc
	g++ -o $@ $^ -std=c++11 -ljsoncpp -lpthread -lseccomp
.PHONY:clean
clean:
	rm -f compile_server