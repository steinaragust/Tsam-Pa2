/* A UDP echo server with timeouts.
 *
 * Note that you will not need to use select and the timeout for a
 * tftp server. However, select is also useful if you want to receive
 * from multiple sockets at the same time. Read the documentation for
 * select on how to do this (Hint: Iterate with FD_ISSET()).
 */

#include <assert.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <glib.h>
#include <libxml/encoding.h>
#include <libxml/xmlwriter.h>
#include <time.h>

void generateheader(char* request, size_t n, GString* response, GHashTable* strain);
void generatehtml(char* request, size_t n, GString* response, GHashTable* strain);
void seed(char* request, size_t n, GHashTable* strain);
char* timestamp();
void logtofile(FILE* logger, const char type, GHashTable* strain);

int main(int argc, char **argv)
{
        int sockfd;
        struct sockaddr_in server, client;
        char message[512];
	char* end;
	FILE* log = fopen("log.txt", "a");
	fclose(log);

	unsigned int port = strtol(argv[1], &end, 10);
	end = (char*)malloc(5);
	end[4] = '\0';
        /* Create and bind a UDP socket */
        sockfd = socket(AF_INET, SOCK_STREAM, 0);
        memset(&server, 0, sizeof(server));
        server.sin_family = AF_INET;
        /* Network functions need arguments in network byte order instead of
           host byte order. The macros htonl, htons convert the values, */
        server.sin_addr.s_addr = htonl(INADDR_ANY);
        server.sin_port = htons(port);
        bind(sockfd, (struct sockaddr *) &server, (socklen_t) sizeof(server));

	/* Before we can accept messages, we have to listen to the port. We allow one
	 * 1 connection to queue for simplicity.
	 */
	listen(sockfd, 1);

        for (;;) {
                fd_set rfds;
                struct timeval tv;
                int retval;

                /* Check whether there is data on the socket fd. */
                FD_ZERO(&rfds);
                FD_SET(sockfd, &rfds);

                /* Wait for five seconds. */
                tv.tv_sec = 5;
                tv.tv_usec = 0;
                retval = select(sockfd + 1, &rfds, NULL, NULL, &tv);

                if (retval == -1) {
                        perror("select()");
                } else if (retval > 0) {
                        /* Data is available, receive it. */
                        assert(FD_ISSET(sockfd, &rfds));

                        /* Copy to len, since recvfrom may change it. */
                        socklen_t len = (socklen_t) sizeof(client);

                        /* For TCP connectios, we first have to accept. */
                        int connfd;
                        connfd = accept(sockfd, (struct sockaddr *) &client,
                                        &len);
                        
                        /* Receive one byte less than declared,
                           because it will be zero-termianted
                           below. */
                        ssize_t n = read(connfd, message, sizeof(message) - 1);
			
			end = (char*)strncpy(end, message, 4);
			//printf("type: %s\nread: %u\n", end, n);
			GHashTable* ogkush = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
			GString* response = g_string_sized_new(1000);
			/* Zero terminate the message, otherwise
 			* printf may access memory outside of the
			* string. */
			message[n] = '\0';
			seed(&message, n, ogkush);
			if(g_strcmp0("GET", end)){
				generateheader(&message, n, response, ogkush);
				generatehtml(&message, n, response, ogkush);
			}
                        /* Send the message back. */
                        write(connfd, response->str, (size_t) response->len);
			logtofile(log, 'g', ogkush);
			response = g_string_free(response, TRUE);
                        /* We should close the connection. */
                        shutdown(connfd, SHUT_RDWR);
                        close(connfd);
			//g_printf("swag: %s\n", strings[0]);
                        /* Print the message to stdout and flush. */
                        fprintf(stdout, "Received:\n%s\n", message);
                        fflush(stdout);
                } else {
                        fprintf(stdout, "No message in five seconds.\n");
                        fflush(stdout);
                }
        }
}

void generatehtml(char* request, size_t n, GString* response, GHashTable* strain){
	//if get
	//g_printf("Port: %s\n", g_hash_table_lookup(strain, "Port"));
	g_string_append(response, "\n");
	g_string_append(response, "<!DOCTYPE html>\n<html>\n<body>\n<p>\n");
	g_string_append(response, "http://");
	g_string_append(response, g_hash_table_lookup(strain, "Address"));
	g_string_append(response, g_hash_table_lookup(strain, "Query"));
	g_string_append(response, "<br>\n");
	g_string_append(response, g_hash_table_lookup(strain, "Address"));
	g_string_append(response, ":");
	g_string_append(response, g_hash_table_lookup(strain, "Port"));
	g_string_append(response, "\n</p>\n</body>\n</html>");
}

void generateheader(char* request, size_t n, GString* response, GHashTable* strain){
	g_string_append(response, "HTTP/1.1 200 OK\n");
	g_string_append(response, "Date: ");
	g_string_append(response, timestamp());
	g_string_append(response, "\n");
	g_string_append(response, "Server: Apache/2\n");
	g_string_append(response, "Content-Length: 16599\n");
	g_string_append(response, "Content-Type: text/html; charset=iso-8859-1\n");
	//g_printf("response: \n%s\n", response->str);
}

void seed(char* request, size_t n, GHashTable* strain){
	g_printf("inni seed");
	gchar** strings = g_strsplit(request, "\n", -1);
	int i = 0;
	gchar** t;
	while(i < 4){
		g_printf("iteration: %d\n", i);	
		if(i == 0){
			t = g_strsplit(strings[0], " ", -1);
			gchar* q = g_strdup(t[1]);
			//g_printf("q: %s\n", q[1]);
			g_hash_table_insert(strain, "Query", q);
			//g_strfreev(t);
		}
		if(i == 2){
			//ch = g_hash_table_lookup(strain, "Host");
			gchar* a;
			gchar* b;
			t = g_strsplit(strings[2], ":", -1);
			a = g_strdup(t[1] + 1);
			b = g_strdup(t[2]);
			printf("b = %s\n", b);
			g_hash_table_insert(strain, "Address", a);
			g_hash_table_insert(strain, "Port", b);
			g_printf("Address: %s\n", g_hash_table_lookup(strain, "Address"));
			//g_printf("Port: %s\n", g_hash_table_lookup(strain, "Port"));
			//g_strfreev(t);
		}
		i += 1;
	}
	//g_strfreev(t);
	//g_printf("here: %s\n", strings[0]);
}

char* timestamp(){
	time_t rawtime;
	struct tm* info;
	char buffer[40];
	rawtime = time(NULL);
	info = localtime(&rawtime);
	strftime(buffer, sizeof(buffer) - 1, "%a, %d %b %Y %I:%M:%S %Z", info);
	//printf("Current local time and date: %s", buffer);
	//Sat, 03 Oct 2015 11:32:14 GMT
	return strdup(buffer);
}

void logtofile(FILE* logger, const char type, GHashTable* strain){
	printf("logfile entry\n");
	char method[5];
	if(type == 'g'){
		strncpy(method, "GET\0", 4);
		g_printf("þetta er get!\n");
	}
	else if(type == 'p'){
		strncpy(method, "POST\0", 5);
	}
	else{
		strncpy(method, "HEAD\0", 5);
	}
	logger = fopen("log.txt", "a");
	g_fprintf(logger, "%s : %s:%s %s %s : %s\n", timestamp(), g_hash_table_lookup(strain, "Address"), g_hash_table_lookup(strain, "Port"), method, g_hash_table_lookup(strain, "Query"), "200");
        fclose(logger);
}
