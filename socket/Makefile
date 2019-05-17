CXXFLAGS= -std=c++11 -pthread
AM_LDFLAGS=  -lboost_system$(BOOST_LIB_SUFFIX) -lboost_filesystem$(BOOST_LIB_SUFFIX) -lboost_program_options$(BOOST_LIB_SUFFIX)
CXX= c++
TARGET= server #client

all: ${TARGET}

server: raceInfo.o server.o
	${CXX} ${CXXFLAGS} -o server $^ ${AM_LDFLAGS}

#client: raceInfo.o client.o
#	${CXX} ${CXXFLAGS} -o client $^ ${AM_LDFLAGS}

%.o: %.cpp
	${CXX} -c ${CXXFLAGS} $*.cpp -o $*.o

clean:
	rm -f *.o *.d
	rm -rf *~ */*~ \#*\#



