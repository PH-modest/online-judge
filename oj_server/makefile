oj_server:oj_server.cc
	g++ -o $@ $^ -std=c++11 -lpthread -lctemplate -ljsoncpp -lmysqlclient -lcrypto
.PHONY:clean
clean:
	rm -f oj_server