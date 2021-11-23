#include <serverutils.h>

static char addrBuffer[MAX_ADDR_BUFFER];
/*
 ** Se encarga de crear el socket pasivo en la dirección correspondiente
 */
int create_socket(char * address, int family, int port, int protocol, int max_pending_connections, char * err_msg) {
	
	log(INFO, "Setting up server socket %s","...");

	// Construct the server address structure
	struct addrinfo addr_criteria;                   	// Criteria for address match
	memset(&addr_criteria, 0, sizeof(addr_criteria)); 	// Zero out structure
	addr_criteria.ai_family = family;             		// Address family
	addr_criteria.ai_flags = AI_PASSIVE;             	// Flags
	addr_criteria.ai_protocol = protocol; 
	
	if (protocol == IPPROTO_TCP) {
		addr_criteria.ai_socktype = SOCK_STREAM;		// Only stream sockets

	} else {
		addr_criteria.ai_socktype = SOCK_DGRAM;
	}

	// Buscamos addr que matcheen con los criterios
	struct addrinfo* serveraddr;
	memset(&serveraddr, 0, sizeof(serveraddr));
	
	char service[10];
	memset(&service, 0, sizeof(service));
	itoa(port, service);

	log(DEBUG, "Getting addr info: address %s, port %s, family %d", address, service, family);
	int rtn = getaddrinfo(address, service, &addr_criteria, &serveraddr);
	if (rtn != 0){
		sprintf(err_msg, "getaddrinfo() failed %s", gai_strerror(rtn));
		return -1;
	}

	int server = -1;
	for (struct addrinfo * addr = serveraddr; addr != NULL && server == -1; addr = addr->ai_next) {

		// Create socket
		//log(INFO, "\nCreating socket %s","...");
		server = socket(family, addr_criteria.ai_socktype, protocol);
		if (server < 0) {
			sprintf(err_msg, "Cant't create socket on %s : %s ", printAddressPort(addr, addrBuffer), strerror(errno));  
			continue;
		}
		
		if(setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &(int) {1}, sizeof(int)) < 0 )
		{
			err_msg = "Set socket options failed";
			close(server);
			server = -1;
			continue;
		}
		
		if(family == AF_INET6 && setsockopt(server, IPPROTO_IPV6, IPV6_V6ONLY, &(int) {1}, sizeof(int)) < 0 )
		{
			err_msg = "Set socket options for IPv6 failed";
			close(server);
			server = -1;
			continue;
		}

		if ((bind(server, addr->ai_addr, addr->ai_addrlen) == 0) && (listen(server, max_pending_connections) == 0)) {
			// Print local address of socket
			struct sockaddr_storage localAddr;
			socklen_t addrSize = sizeof(localAddr);
			if (getsockname(server, (struct sockaddr *) &localAddr, &addrSize) >= 0) {
				printSocketAddress((struct sockaddr *) &localAddr, addrBuffer);
				log(INFO, "Binding to %s", addrBuffer);
			}
		}
		if ((bind(server, addr->ai_addr, addr->ai_addrlen) == 0) && (listen(server, max_pending_connections) != 0) && (protocol == IPPROTO_TCP)) {
			sprintf(err_msg, "Cant't bind %s", strerror(errno));  
			close(server);  // Close and try with the next one
			server = -1;
		}
	}

    log(INFO, "Socket for %s in fd %d created with succes.", address, server);
    
	freeaddrinfo(serveraddr);

	return server;
}
