#pragma once
#include <iostream>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <time.h>
#include <chrono>

// define constants 
#define MAX_BUFFER 4096
#define MAX_EVENTS 10000


class Server {
	int epfd;
	int server_fd;

	char buffer[MAX_BUFFER];
	size_t currentClientsCount;
	size_t totalClientCount;
	bool shutdownRequested = false;

public:
	Server() : currentClientsCount(0), totalClientCount(0), shutdownRequested(false) {}
	void start();
	size_t getCurrentClientCount() const{ return currentClientsCount; };
	size_t getTotalClientCount() const { return totalClientCount; };
};


