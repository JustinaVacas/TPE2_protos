#include "pop3client.h"

#define STATS "STATS"
#define GET_TIMEOUT "GET_TIMEOUT"
#define SET_TIMEOUT "SET_TIMEOUT"

struct sockaddr_in serverAddr;

char * toBuffer(int read, char * buffer){
    char *command = malloc(CMD_LEN);            //esto va a necesitar free
	int i;
    for(i = 0; i < read && i < CMD_LEN; i++) {
        if (buffer[i] != ' ') {
            command[i] = toupper(buffer[i]);
        }
        else{
            command[i] = ' ';
        }
    }
    command[i-1] = 0;
    return command;
}

void set_requestBuffer(char cmd, char * request, int i, int read, char * stdin_read){
    request[CMD_LEN - 2] = cmd;
    int j;
    for (j = (CMD_LEN-1); i < read; i++, j++){
        request[j] = stdin_read[i];
    }
    memcpy(request + j, "\r\n", 2);
}

int cmp_admin(uint8_t * str1, uint8_t * str2){
    for (int i = 0; i < 2; i++) {
        if (str1[i] != str2[i]) 
            return 1;
    }   
    return 0;
}
            
static int get_command(const char* command) {
    if(strcmp(command, STATS) == 0){
        return 0;
    }
    if(strcmp(command, GET_TIMEOUT) == 0){
        return 1;
    }
    if(strcmp(command, SET_TIMEOUT) == 0){
        return 2;
    }
    return -1;
}

// del TP1
int udpSocket(int port){
	int sock;
	if ( (sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) {
		log(ERROR, "UDP socket creation failed, errno: %d %s", errno, strerror(errno));
		return sock;
	}
	log(DEBUG, "UDP socket %d created", sock);
	memset(&serverAddr, 0, sizeof(serverAddr));
	serverAddr.sin_family    = AF_INET;         // IPv4cle
	serverAddr.sin_addr.s_addr = INADDR_ANY;
	serverAddr.sin_port = htons(port);
	return sock;
}

static int parse(admin_response * response, uint8_t * buffer, int buff_len){
    response->status =  *buffer;
    buffer = buffer + 1;
    memcpy(response->data, buffer, buff_len - 2 - 1);
    response->data[buff_len - 2 - 1] = '\0';
    buffer += buff_len - 2 - 1;
    if (!cmp_admin(buffer, (uint8_t *) "\r\n")){
        return 1;
    }
    fprintf(stderr, "Command not found\n");
    return 0;
}


static void print_help(){
	printf(
            "\n-------------------------- HELP --------------------------\n"
            " - STATS 			Devuelve las metricas (conecciones historicas, coneccion actual, bytes transferidos). \n"
            " - GET_TIME OUT	Devuelve el valor del time out\n"
            " - SET_TIME OUT 	Setea el valor del time out\n"
            "\n"
        );
}

struct client_args * client_args;

int main(int argc, char* argv[]){
    char buffer[DGRAM_SIZE];
	ssize_t n;
    socklen_t clntAddrLen = sizeof(serverAddr);
    int read;
    char * command;
    int cmd;
    int i;
    char request[DGRAM_SIZE] = {0};
    char * stdin_read = NULL;
    size_t stdin_len = 0;
    client_args = malloc(sizeof(struct client_args));
    
    if(client_args == NULL){
        fprintf(stderr,"Malloc failure\n");
        return -1;
    }
    parse_args(argc, argv,client_args);
    if(client_args->admin_auth == NULL){
        fprintf(stderr,"Auth is required\n");
        return -1;
    }
    memcpy(request, client_args->admin_auth, 5);

    log(DEBUG,"Client connect to %s:%d using token %s\n",
        client_args->admin_server_address, client_args->admin_server_port, client_args->admin_auth);

    // Socket UDP
	int udpSock = udpSocket(client_args->admin_server_port);
	if (udpSock < 0) {
		log(ERROR, "UDP socket failed, errno: %d %s", errno, strerror(errno));
		exit(EXIT_FAILURE);
	} else {
		log(DEBUG, "Waiting for UDP IPv4 on socket %d\n", udpSock);
	}
	while(TRUE){
        read = getline(&stdin_read, &stdin_len, stdin);
        if(read < 0){
            log(ERROR, "Error reading: %d %s", errno, strerror(errno));
        }
        command = toBuffer(read, stdin_read);

        if(strcmp("HELP", command) == 0){
            print_help();
            continue;
        }
        if((cmd = get_command(command)) < 0){
            continue;
        }      
        i = strlen(command);
        set_requestBuffer((char) cmd, request, i,read, stdin_read);
        if(sendto(udpSock, (const char*) request, strlen(request), 0, (const struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0){
            perror("Error request");
            return -1;
        }
        if((n = recvfrom(udpSock, (char*) buffer, MAX_LINE, 0, (struct sockaddr *) &serverAddr, &clntAddrLen)) < 0){
            perror("Error");
            return -1;
        }
        admin_response response;
        if(parse(&response, (uint8_t *) buffer, n) != 0){
            printf("Invalid response");
            continue;
        }
        //print status
        if(response.status != 0){
            printf("-ERR: %s\n", (char*) response.data);
        } else{
            printf("+OK\n%s\n", (char*) response.data);
        }
    }
    return 0;
}
