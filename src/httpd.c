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
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <glib.h>
#include <libxml/encoding.h>
#include <libxml/xmlwriter.h>
#include <time.h>

void generateheader(size_t n, GString* response, GHashTable* strain);
void generatehtml(size_t n, GString* response, GHashTable* strain, const char type);
void seed(char* request, struct sockaddr_in* client, size_t n, GHashTable* strain, const char type);
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
	GTimer* pers = NULL;
	gulong* elapsed = NULL;
	gpointer connection = NULL;
	int connfd;
	int alive = 0;

	unsigned int port = strtol(argv[1], &end, 10);
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
                }
                else if (retval > 0) {
                    /* Data is available, receive it. */
                    assert(FD_ISSET(sockfd, &rfds));

                    /* Copy to len, since recvfrom may change it. */
                    socklen_t len = (socklen_t) sizeof(client);

                    /* For TCP connectios, we first have to accept. */
                    if(alive == 0){
                    	connfd = accept(sockfd, (struct sockaddr *) &client, &len);
                    	alive = 1;
                    }
                    
                    /* Receive one byte less than declared,
                       because it will be zero-termianted
                       below. */
                    ssize_t n = read(connfd, message, sizeof(message) - 1);
                    g_printf("n read: %zu\n", n);
		            if(n > 0){
						GHashTable* ogkush = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
						GString* response = g_string_sized_new(1000);
						/* Zero terminate the message, otherwise
			 			* printf may access memory outside of the
						* string. */
						message[n] = '\0';
						char type;
						if(message[0] == 'G'){
							type = 'g';
						}
						else if(message[0] == 'P'){
							type = 'p';
						}
						else{
							type = 'h';
						}

						seed(&message, &client, n, ogkush, type);
						generateheader(n, response, ogkush);
						//g_printf("type: %c\n", type);
						if(type != 'h'){
							generatehtml(n, response, ogkush, type);
						}
			            /* Send the message back. */
			            write(connfd, response->str, (size_t) response->len);
						logtofile(log, type, ogkush);
						connection = g_hash_table_lookup(ogkush, "Connection");
						response = g_string_free(response, TRUE);
						g_hash_table_remove_all(ogkush);
						/* Print the message to stdout and flush. */
	                    fprintf(stdout, "Received:\n%s\n", message);
	                    fflush(stdout);
					}
		            /* We should close the connection. */
		            if(connection == NULL || g_strcmp0("close", connection) == 0 || (pers != NULL && g_timer_elapsed(pers, elapsed) >= 10)){
		            	g_printf("closes\n");
		            	alive = 0;
		            	if(pers != NULL){
		            		g_timer_destroy(pers);
		            	}
		            	pers = NULL;
			            shutdown(connfd, SHUT_RDWR);
			            close(connfd);
			        }
			        else{
			        	g_printf("keeps alive\n");
			        	connection = NULL;
			        	if(pers == NULL){
			        		pers = g_timer_new();
			        	}
			        	else{
			        		g_timer_reset(pers);
			        	}
			        }
                }
                else if((pers != NULL && g_timer_elapsed(pers, elapsed) >= 10)){
                	printf("closes\n");
                	alive = 0;
	            	g_timer_destroy(pers);
	            	pers = NULL;
		            shutdown(connfd, SHUT_RDWR);
		            close(connfd);
                }
                else {
                    fprintf(stdout, "No message in five seconds.\n");
                    fflush(stdout);
                    if(pers != NULL){
                    	g_printf("g_timer_elapsed %f\n", g_timer_elapsed(pers, elapsed));
                    }
                }
        }
}

void generatehtml(size_t n, GString* response, GHashTable* strain, const char type){
	g_string_append(response, "\n");
	g_string_append(response, "<!DOCTYPE html>\n<html>\n<body>\n<p>\nhttp://");
	g_string_append(response, g_hash_table_lookup(strain, "Host"));
	g_string_append(response, g_hash_table_lookup(strain, "Query"));
	//g_printf("lengd 1 strengs: %u\n", response->len);
	g_string_append(response, "<br>\n");
	//g_printf("lengd 2 strengs: %u\n", response->len);
	g_string_append(response, g_hash_table_lookup(strain, "Client-Address"));
	//g_printf("lengd 3 strengs: %u\n", response->len);
	g_string_append(response, ":");
	g_string_append(response, g_hash_table_lookup(strain, "Port"));
	g_string_append(response, "\n</p>");
	if(type == 'p'){
		g_string_append(response, "\n<p>\n");
		g_string_append(response, g_hash_table_lookup(strain, "Post-Content"));
		g_string_append(response, "\n</p>");
	}
	g_string_append(response, "\n</body>\n</html>\n");
	//g_printf("html: %s\n", response->str);
}


void generateheader(size_t n, GString* response, GHashTable* strain){
	g_string_append(response, "HTTP/1.1 200 OK\n");
	g_string_append(response, "Date: ");
	g_string_append(response, timestamp());
	g_string_append(response, "\n");
	g_string_append(response, "Server: Apache/2\n");
	//g_string_append(response, "Content-Length: 16599\n");
	g_string_append(response, "Content-Type: text/html; charset=iso-8859-1\n");
	//g_printf("response: \n%s\n", response->str);
}

void seed(char* request, struct sockaddr_in* client, size_t n, GHashTable* strain, const char type){
	gchar** strings = g_strsplit(request, "\n", -1);
	int i = 1;
	gchar** t;
	t = g_strsplit(strings[0], " ", -1);
	gchar* q = g_strdup(t[1]);
	g_hash_table_insert(strain, strdup("Query"), q);
	while(i < g_strv_length(strings) - 3){
		t = g_strsplit(strings[i], ":", -1);
		gchar* a = g_strdup(t[1] + 1);
		//g_printf("key: %s\n", t[0]);
		g_hash_table_insert(strain, g_strdup(t[0]), a);
		i += 1;
	}
	uint ip = client->sin_addr.s_addr;
	gchar* a = (gchar*)g_malloc(INET_ADDRSTRLEN);
	inet_ntop(AF_INET, &ip, a, INET_ADDRSTRLEN);
	g_hash_table_insert(strain, g_strdup("Client-Address"), a);
	uint port = htons(client->sin_port);
	gchar* b = (gchar*)g_malloc(6);
	g_sprintf(b, "%u\0", port);
	g_hash_table_insert(strain, g_strdup("Port"), b);
	/*g_printf("Port here: %u", g_hash_table_lookup(strain, "Port"));
	g_printf("Address: %s\n", g_hash_table_lookup(strain, "Address"));*/

	if(type == 'p'){
		g_hash_table_insert(strain, g_strdup("Post-Content"), g_strdup(strings[g_strv_length(strings) - 1]));
		/*g_printf("Content-Length: %s\n", g_hash_table_lookup(strain, "Content-Length"));
		g_printf("Content-Type: %s\n", g_hash_table_lookup(strain, "Content-Type"));
		g_printf("Post-Content: %s\n", g_hash_table_lookup(strain, "Post-Content"));*/
	}
	g_strfreev(t);
}

char* timestamp(){
	time_t rawtime;
	struct tm* info;
	char buffer[40];
	rawtime = time(NULL);
	info = localtime(&rawtime);
	strftime(buffer, sizeof(buffer) - 1, "%a, %d %b %Y %I:%M:%S %Z", info);
	return strdup(buffer);
}

void logtofile(FILE* logger, const char type, GHashTable* strain){
	char method[5];
	if(type == 'g'){
		strncpy(method, "GET\0", 4);
	}
	else if(type == 'p'){
		strncpy(method, "POST\0", 5);
	}
	else{
		strncpy(method, "HEAD\0", 5);
	}
	logger = fopen("log.txt", "a");
	/*g_printf("1, %s\n", g_hash_table_lookup(strain, "Client-Address"));
	g_printf("2, %s\n", g_hash_table_lookup(strain, "Port"));
	g_printf("3, %s\n", g_hash_table_lookup(strain, "Query"));
	g_printf("4, %s\n", timestamp());
	g_printf("5, %s\n", method);*/
	g_fprintf(logger, "%s : %s:%s %s %s : %s\n", timestamp(), g_hash_table_lookup(strain, "Client-Address"), g_hash_table_lookup(strain, "Port"), method, g_hash_table_lookup(strain, "Query"), "200");
    fclose(logger);
}