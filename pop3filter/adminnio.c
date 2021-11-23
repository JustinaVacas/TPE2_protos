#include "adminnio.h"

void 
sent(int fd, char * response,size_t n,struct sockaddr_storage client_addr, socklen_t client_addr_len){
    if (sendto(fd, response, n, 0, (const struct sockaddr *) &client_addr, client_addr_len) < 0)
		log(DEBUG, "%s", "Error sending response");
}

int
cmp_admin(uint8_t * str1, uint8_t * str2){
    for (int i = 0; i < SIZE-1; i++) {
        if (str1[i] != str2[i]) 
            return 1;
    }   
    return 0;
}

//************************************** STATS **************************************

void 
stats_handler(int fd, int request_len, struct admin_request * req, struct sockaddr_storage client_addr, size_t client_addr_len) {
    char response[DGRAM_SIZE];
    size_t n = sprintf(response,"Total connections: %llu\nActive connections: %llu\nTransferred bytes to client: %llu\nTransferred bytes to origin: %llu\r\n",
    proxy_metrics.totalConnections, proxy_metrics.activeConnections, proxy_metrics.totalBytesToClient, proxy_metrics.totalBytesToOrigin);

    sent(fd, response, n, client_addr, client_addr_len);
}

//************************************** TIME OUT **************************************

void 
get_timeout_handler(int fd, int request_len, struct admin_request * request, struct sockaddr_storage client_addr, size_t client_addr_len){
    char response[DGRAM_SIZE];
    size_t n = sprintf(response,"timeout: %.3f\r\n",time_out);

    sent(fd, response, n, client_addr, client_addr_len);
}

void 
set_timeout_handler(int fd, int request_len, struct admin_request * request, struct sockaddr_storage client_addr, size_t client_addr_len){
    char time[DGRAM_SIZE - MIN_DGRAM - 1];
    int i;
    for (i = 0; i < DGRAM_SIZE - MIN_DGRAM; i++){
        time[i] = (char) request->data[i];
        if(request->data[i] == '\r'){
            break;
        }
    }
    time[i] = 0;
    time_out = atof(time);
}

//************************************** ADMIN COMMANDS **************************************

void(* admin_commands[COMMAND_SIZE])(int, int, struct admin_request *, struct sockaddr_storage, size_t) = 
    { 
        stats_handler, 
        get_timeout_handler, 
        set_timeout_handler, 
    };

//TODO cambiar nombre de la funcion a admin
void 
admin_passive_accept(struct selector_key *key){
    
    log(DEBUG, "%s", "Incoming admin datagram");
    
    uint8_t buffer[DGRAM_SIZE];
	int read_buffer;
    struct sockaddr_storage  client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

	read_buffer = recvfrom(key->fd, buffer, DGRAM_SIZE, 0, (struct sockaddr *) &client_addr, &client_addr_len);
    if(read_buffer <= 0) {
        log(ERROR, "%s", "Error reading datagram");
        return;
    }

    if(read_buffer < MIN_DGRAM) {
        sent(key->fd, "Not enough parameters\r\n", 24, client_addr, client_addr_len);
        return;
    }

    admin_request * request = (admin_request *) buffer;
    request->command = request->command - ASCII;
    log(DEBUG, "auth %s",request->auth);
    log(DEBUG, "command %d",request->command);
    log(DEBUG, "data %s",request->data);

    if ((cmp_admin(ADMIN_AUTH, request->auth) != 0) || (request->command >= COMMAND_SIZE) ) {
        sent(key->fd, "Error in parameters\r\n", 19, client_addr, client_addr_len);
        return;
    }

    admin_commands[request->command](key->fd, read_buffer, request, client_addr, client_addr_len);

}

