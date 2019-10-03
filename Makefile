
#CC=clang --analyze
#CXX=clang++ --analyze
CC=gcc
CXX=g++
#CC=i686-pc-mingw32-gcc
#CXX=i686-pc-mingw32-g++
#CC=i686-pc-cygwin-gcc
#CXX=i686-pc-cygwin-gcc

BINDIR=./bin
CFLAGS=-g -IExternalProjects/safelist -IExternalProjects/threadutils

SERVERSOURCEFILENAMES=servermain.cpp fdutils.cpp socketconnection_base.cpp tlssocketconnection.cpp serversocket.cpp packet.cpp debugger.cpp
SERVEROBJECTS=$(SERVERSOURCEFILENAMES:.cpp=.o)

CLIENTSOURCEFILENAMES=clientmain.cpp fdutils.cpp socketconnection_base.cpp tlssocketconnection.cpp clientsocket.cpp packet.cpp debugger.cpp
CLIENTOBJECTS=$(CLIENTSOURCEFILENAMES:.cpp=.o)

all : $(SERVERSOURCES) server $(CLIENTSOURCES) client
	
	
server : $(SERVEROBJECTS)
	$(CXX) $(SERVEROBJECTS) -lpthread -lssl -lcrypto -o server

client : $(CLIENTOBJECTS)
	$(CXX) $(CLIENTOBJECTS) -lpthread -lssl -lcrypto -o client

.cpp.o :
	$(CXX) $(CFLAGS) -c $< -o $@
	
clean :
	rm -f $(SERVEROBJECTS) $(CLIENTOBJECTS) server client

	
