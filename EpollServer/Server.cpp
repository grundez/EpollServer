// TODO 
// ADD UDP FILE DESCRIPTORS
// ADD CLIENT CONNECTION SET -> CREATE MESSAGE METHOD THEN SERVER IS DOWN
// RELOCATE BLOCKS OF CODE IN METHODS


#include "Server.h"

void Server::start() {
	// create server socket
	server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if(server_fd == -1){
		std::cerr << "Failed to create socket" << std::endl;
		exit(-1);
	}

	// make epoll non blockable and reusable by same address
	int opt = 1;
	setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	int flags = fcntl(server_fd, F_GETFL, 0); // save current flags
	fcntl(server_fd, F_SETFL, flags | O_NONBLOCK);

	// configure server addr
	struct sockaddr_in server_addr{};
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_port = htons(8000);

	// bind server address to socket
	if(bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1){
		std::cerr << "Failed to bind socket" << std::endl;
		close(server_fd);
		exit(-1);	
	}

	// start listening connections
	if(listen(server_fd, SOMAXCONN) == -1){
		std::cerr << "Failed to listen connection" << std::endl;
		close(server_fd);
		exit(-1);
	}

	// create epoll instance
	epfd = epoll_create(1);
	if(epfd == -1){
		std::cerr << "Failed to create epoll intance" << std::endl;
		close(server_fd);
		exit(-1);
	}

	// create event struct 
	struct epoll_event event;
	event.data.fd = server_fd;
	event.events = EPOLLIN; // looking for writes events

	// add server events fd to epoll file descriptor
	if(epoll_ctl(epfd, EPOLL_CTL_ADD, server_fd, &event) == -1){
		std::cerr << "Failed to create epoll intance" << std::endl;
		close(epfd);
		close(server_fd);
		exit(-1);
	}

	std::cout << "Server started on port: 8000" << std::endl;;
	struct epoll_event events[MAX_EVENTS];
	while(!shutdownRequested){
		// waiting for new events
		int eventsCount = epoll_wait(epfd, events, MAX_EVENTS, 10);
		if(eventsCount < 0){
			if(errno == EINTR && shutdownRequested) {
                // break by signal
                break;
            }

			std::cerr << "Failed to wait events" << std::endl;
			break;
		}

		// if shutdoned and has no events
		if(shutdownRequested && eventsCount == 0) {
            break;
        }

		for(int i = 0; i < eventsCount; ++i){
			// case when server has new connection: event fd == server fd
			if(events[i].data.fd == server_fd){
				// increment client counter for staticstic
				currentClientsCount++;
				totalClientCount++;

				struct sockaddr_in client_addr;
				socklen_t client_len = sizeof(client_addr);

				int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
				if(client_fd == -1){
					std::cerr << "Failed to create client file descriptor" << std::endl;
					continue;
				}

				// make epoll non blockable
				int flags = fcntl(server_fd, F_GETFL, 0); // save current flags
				fcntl(server_fd, F_SETFL, flags | O_NONBLOCK);
				std::cout << "New client connected: " << client_fd << std::endl;

				// message client what he is connected
				const char* welcomeMsg = "Connected to server!\n";
				write(client_fd, welcomeMsg, strlen(welcomeMsg));

				// crate event struct for client fd
				struct epoll_event client_event;
				client_event.data.fd = client_fd;
				client_event.events = EPOLLIN;

				// add client fd to epoll fd
				if(epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &client_event) == -1){
					std::cerr << "Failed to add client " << client_fd <<" to epoll" << std::endl;
					currentClientsCount--;  // decrement counters if failed
					totalClientCount--;
					close(client_fd);
					continue;
				}

			}
			else{
				// case when received client event
				int client_fd = events[i].data.fd;
				
				// reading messages from client
    			ssize_t read_bytes = read(client_fd, buffer, MAX_BUFFER - 1);

				if(read_bytes > 0){
					buffer[read_bytes] = '\0';

					std::string receivedMessage{buffer};
					// delete /r/n from back of message
					if(!receivedMessage.empty() && receivedMessage.back() == '\n') receivedMessage.pop_back();
					if(!receivedMessage.empty() && receivedMessage.back() == '\r') receivedMessage.pop_back();

					std::cout << "Received from client " << client_fd << ": " << receivedMessage << std::endl;

					bool isCommand = false;
					std::string response;

					if(receivedMessage == "/time"){
						// get current time
						auto now = std::chrono::system_clock::now();
						std::time_t time = std::chrono::system_clock::to_time_t(now);
						response = "Server time: " + std::string(std::ctime(&time));
            			isCommand = true;
					}
					else if(receivedMessage == "/stats"){
						std::string currentClientCountStr = std::to_string(currentClientsCount);
						std::string totalClientCountStr = std::to_string(totalClientCount);

						response = "Current client connected: " + currentClientCountStr;
						response += "\nTotal client connections: " + totalClientCountStr + "\n";
						isCommand = true;
					}
					else if(receivedMessage == "/disconnect"){
						response = "Disconnected!\n";
						write(client_fd, response.c_str(), response.length());	
						std::cout << "Disconnect command received from client: " << client_fd << " - disconnected!" << std::endl;
						
						// disconnect client from server
						currentClientsCount--;
						epoll_ctl(epfd, EPOLL_CTL_DEL, client_fd, nullptr);
						close(client_fd);
						continue;
					}
					else if(receivedMessage == "/shutdown"){
						response = "Server shutting down...\nDisconnected!\n";
						write(client_fd, response.c_str(), response.length());	
						std::cout << "Shutdown server command received from client: " << client_fd << std::endl;
						
						// shutdown sesrver
						close(server_fd);
						shutdownRequested = true;
						break;
					}
					else if(!receivedMessage.empty() && receivedMessage[0] == '/'){
						response = "Unknown command: " + receivedMessage;
						response +=  "\nAvailable commands: \n/time - check current time\n/stats - check connected clients stats\n/disconnect - disconnect from server\n/shutdown - shutdown server\n";
						isCommand = true;
					}
					else{
						// echo answer for default messages
						response = receivedMessage + "\n";
					}

					// send response
					write(client_fd, response.c_str(), response.length());
				} 
				else if (read_bytes == 0 || (read_bytes < 0 && errno != EAGAIN)) {
                    // client disconnected or get error
					currentClientsCount--;
                    std::cout << "Client disconnected: " << client_fd << std::endl;
                    epoll_ctl(epfd, EPOLL_CTL_DEL, client_fd, nullptr);
                    close(client_fd);
                }
			}
		}
	}
	
	close(epfd);
	close(server_fd);
}


int main(){
	Server serv;
	serv.start();
	return 0;
}
