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
	while(true){
		// waiting for new events
		int eventsCount = epoll_wait(epfd, events, MAX_EVENTS, -1);
		if(eventsCount < 0){
			std::cerr << "Failed to wait events" << std::endl;
			break;
		}

		for(int i = 0; i < eventsCount; ++i){
			// case when server has new connection: event fd == server fd
			if(events[i].data.fd == server_fd){
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
				const char* welcome_msg = "Connected to server!\n";
				write(client_fd, welcome_msg, strlen(welcome_msg));

				// crate event struct for client fd
				struct epoll_event client_event;
				client_event.data.fd = client_fd;
				client_event.events = EPOLLIN;

				// add client fd to epoll fd
				if(epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &client_event) == -1){
					std::cerr << "Failed to add client " << client_fd <<" to epoll" << std::endl;;
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
					std::cout << "Received from client " << client_fd << ": " << buffer;

					if(buffer[0] == '/'){
						std::cout << "Command received from client " << client_fd << ": " << buffer;		

						// TODO make command parser 
					}

					// echo answer
					write(client_fd, buffer, read_bytes);
				} 
				else if (read_bytes == 0 || (read_bytes < 0 && errno != EAGAIN)) {
                    // Клиент отключился или ошибка
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
