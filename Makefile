CXX = g++
CXXFLAGS = -std=c++11 -Wall -g
LDFLAGS = -lpthread -lmysqlclient

SERVER_OBJ = server.o threadpool.o mysql_db.o
CLIENT_OBJ = client.o
SIMPLE_CLIENT_OBJ = simple_client.o

all: server client simple_client

server: $(SERVER_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

client: $(CLIENT_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ -lpthread

simple_client: $(SIMPLE_CLIENT_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ -lpthread

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f *.o server client simple_client

.PHONY: all clean