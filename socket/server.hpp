#ifndef SERVER_INCLUDE
#define SERVER_INCLUDE


#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <stdio.h>
#include <malloc.h>
#include <time.h>
#include <netdb.h>
#include <errno.h>

#include <iostream>
#include <boost/asio.hpp>
#include "raceInfo.hpp"

//送信用にレース情報を整形
void setRaceInfoForSend(RaceInfo &info, string &all, int length, int width);

#endif