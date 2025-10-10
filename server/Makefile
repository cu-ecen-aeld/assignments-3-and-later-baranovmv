CFLAGS=-g -Wall -Werror

all: server

server: server.cpp
	$(CROSS_COMPILE)$(CXX) $(CFLAGS) $(INCLUDES) $(OBJS) server.cpp -o aesdsocket $(LDFLAGS)

clean:
	rm -f server  *.o

