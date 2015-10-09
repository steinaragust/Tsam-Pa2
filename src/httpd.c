/* Not a UDP echo server with timeouts.
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
#include <errno.h>
#include <sys/epoll.h>

/*Declare functions*/
void generateheader(GString* response, GHashTable* strain);
void generatehtml(GString* response, GHashTable* strain, const char type);
void seed(char* request, struct sockaddr_in* client, GHashTable* strain, const char type);
char* timestamp();
void logtofile(FILE* logger, const char type, GHashTable* strain);

/*"Impicit declerations"*/
void g_printf (gchar const *format, ...);
gint g_fprintf (FILE *file, gchar const *format, ...);

int main(int argc, char **argv)
{
	if(argc != 2){
		g_printf("I take one argument!\n");
		exit(1);
	}
	/*Declare variables*/
    int sockfd;
    struct sockaddr_in server, client;
    fd_set rfds, temp;
    char message[512];
	char* end;
	/*Open the log, it is automatically created if it does not exist*/
	FILE* log = fopen("httpd.log", "a");
	fclose(log);
	int connfd;
	int maxfd;
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

	/*The parallel connection part of this program is highly influenced on part 4 of the
	link http://developerweb.net/viewtopic.php?id=2933 provided on piazza, 5 connections work
	according the the python script provided by Marcel*/

    /* Check whether there is data on the socket fd. Set the socket and assign the variable
    which will increment on each new connection.*/
    FD_ZERO(&rfds);
    FD_SET(sockfd, &rfds);
    maxfd = sockfd;


        for (;;) {
        		/*Duplicate memory between structs*/
        		memcpy(&temp, &rfds, sizeof(temp));
                struct timeval tv;
                int retval;
                /* Wait for five seconds. */
                tv.tv_sec = 5;
                tv.tv_usec = 0;
                /*Call select for next fd*/
                retval = select(maxfd + 1, &temp, NULL, NULL, &tv);
                g_printf("retval: %d\n", retval);
                if (retval == -1) {
                    perror("select()");
                }
                else if (retval > 0) {
                    /* Check if the selected file descriptor is a part of the set */
                    if(FD_ISSET(sockfd, &temp)){
                    	/* Copy to len, since recvfrom may change it. */
                    	socklen_t len = (socklen_t) sizeof(client);
                    	/* For TCP connectios, we first have to accept. */
                    	connfd = accept(sockfd, (struct sockaddr *) &client, &len);
                    	if (connfd < 0) {
				            printf("Error in accept(): %s\n", strerror(errno));
				        }
				        /*If connection is accepted, then we add the file descriptor
				        to the set and increment the maximum file descriptor if neccessary*/
				        else{
				        	FD_SET(connfd, &rfds);
				        	maxfd = (maxfd < connfd)?connfd:maxfd;
				        }
				        /* Clear this fd from the set after being added to the other*/
				        FD_CLR(sockfd, &temp);
                    }
                    /*For loop that iterates through each file descriptor that is set*/
                    int j;
                    for(j = 0; j < maxfd + 1; j++){
                    	if(FD_ISSET(j, &temp)){
                    		/*Memsetting everything is a good practice*/
                    		memset(message, 0, sizeof(message));
                    	/* Receive one byte less than declared,
	                       because it will be zero-termianted
	                       below. */
	                       ssize_t n = read(j, message, sizeof(message) - 1);
                    		/*Only realistic values read from the file descriptor are */
           					if(n > 0 && n <= 511){
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
								/*Proccess the message depending on the type*/
								seed((char*)&message, &client, ogkush, type);
								generateheader(response, ogkush);
								if(type != 'h'){
									generatehtml(response, ogkush, type);
								}
					            /* Send the message back. */
					            write(j, response->str, (size_t) response->len);
								logtofile(log, type, ogkush);
								/*Free the string we just sent*/
								g_string_free(response, TRUE);
								/*Destroy the hash table, the beauty of this is that it free's all pointers that were allocated
								for the table, so we allocate all we want into this table and then only have to call free once*/
								g_hash_table_remove_all(ogkush);
								g_hash_table_destroy(ogkush);
								/* Print the message to stdout and flush. */
			                    fprintf(stdout, "Received:\n%s\n", message);
			                    fflush(stdout);
			                    /*Close the connection and remove the file descriptor from the set, i did not manage to make the persistent
			                    connections, but they should be parallel*/
			                    shutdown(j, SHUT_RDWR);
			                    close(j);
			                    FD_CLR(j, &rfds);
           					}
                    	}
                    }
                }
        }
}
/*The document is generated, depending on which type of document it is or what was
in the query string*/
void generatehtml(GString* response, GHashTable* strain, const char type){
	g_string_append(response, "\n");
	g_string_append(response, "<!DOCTYPE html>\n<html>\n");
	gchar* p = g_hash_table_lookup(strain, "Color");
	gchar* cook = g_hash_table_lookup(strain, "Cookie");
	/*If a new color was set in the query string, then we generate a empty html only with the background set*/
	if(p != NULL){
		g_string_append(response, "<body style=\"background-color:");
		g_string_append(response, p);
		g_string_append(response, "\">");
	}
	/*If the query url was: "/color", we check if the request header from the browser included a field which had a cookie
	in it, this cookie only included what color the background should be set to. Note that if the query url was "/color" and a cookie
	has not yet been set then we treat this as a normal get query*/
	else if(cook != NULL && g_hash_table_lookup(strain, "Query") != NULL && g_strcmp0(g_hash_table_lookup(strain, "Query") + 1, "color") == 0){
		g_string_append(response, "<body style=\"background-color:");
		g_string_append(response, cook + 3);
		g_string_append(response, "\">");
	}
	/*If we are not requesting a background html, we either have a normal get request, get request with arguments or a post
	request.*/
	else{
		/*Starts with appending the usual lines for the html*/
		g_string_append(response, "<body>\n<p>\nhttp://");
		gchar** rickjames = g_strsplit(g_hash_table_lookup(strain, "Host"), ":", -1);
		g_string_append(response, rickjames[0]);
		g_strfreev(rickjames);
		g_string_append(response, g_hash_table_lookup(strain, "Query"));
		//g_printf("lengd 1 strengs: %u\n", response->len);
		g_string_append(response, "<br>\n");
		//g_printf("lengd 2 strengs: %u\n", response->len);
		g_string_append(response, g_hash_table_lookup(strain, "Client-Address"));
		//g_printf("lengd 3 strengs: %u\n", response->len);
		g_string_append(response, ":");
		g_string_append(response, g_hash_table_lookup(strain, "Port"));
		p = g_hash_table_lookup(strain, "NArgs");
		/*If there were arguments in the query string, we append them into the html*/
		if(p != NULL){
			g_string_append(response, "<br>\n");
			/*Convert the number into a integer from string*/
			int n = atoi((char*)p);
			int i = 0;
			/*The key and value strings are not bigger than 10 bytes*/
			gchar* buf = (gchar*)malloc(10);
			gchar* key;
			gchar* value;
			while(i < n - 1){
				/*Create the strings which are going to be the keys and values we are looking for in
				the hash table, as they parsed in the seed function*/
				g_snprintf(buf, 10, "arg%u", i);
				key = g_hash_table_lookup(strain, buf);
				g_snprintf(buf, 10, "val%u", i);
				value = g_hash_table_lookup(strain, buf);
				g_string_append(response, key);
				g_string_append(response, "=");
				g_string_append(response, value);
				g_string_append(response, "<br>\n");
				i++;
			}
			/*The last argument is a special case, because it does not contain a "<br" at the end of the line*/
			g_snprintf(buf, 10, "arg%u", i);
			key = g_hash_table_lookup(strain, buf);
			g_snprintf(buf, 10, "val%u", i);
			value = g_hash_table_lookup(strain, buf);
			g_string_append(response, key);
			g_string_append(response, "=");
			g_string_append(response, value);
			g_free(buf);
		}
		g_string_append(response, "\n</p>");
		/*Note that a post request does not work with arguments, it is printed the way the instructor talked about
		on piazza, a request of the type "curl -d "this is post content, i.e paylod in http package" localhost:10000" should work
		like shown on piazza, it is not more fine grained than that.*/
		if(type == 'p'){
			g_string_append(response, "\n<p>\n");
			g_string_append(response, g_hash_table_lookup(strain, "Post-Content"));
			g_string_append(response, "\n</p>");
		}
	}
	g_string_append(response, "\n</body>\n</html>\n");
}

/*Most of the headers are harcoded as seen, only the timestamp is allocated and appended
to the header, and then only if there was a new background color sent in the request, we 
send the client a new cookie to be set*/
void generateheader(GString* response, GHashTable* strain){
	g_string_append(response, "HTTP/1.1 200 OK\n");
	g_string_append(response, "Date: ");
	char* t = timestamp();
	g_string_append(response, timestamp());
	g_free(t);
	g_string_append(response, "\n");
	g_string_append(response, "Server: Apache/2\n");
	g_string_append(response, "Content-Type: text/html; charset=iso-8859-1\n");
	gchar* p = g_hash_table_lookup(strain, "Color");
	if(p != NULL){
		  g_string_append(response, "Set-Cookie: ");
		  g_string_append(response, "bg = ");
		  g_string_append(response, g_hash_table_lookup(strain, "Color"));
		  g_string_append(response, "\n");
	  }
}


/*This function parses everything into the hash table, it is not pefect and might fail on edge cases, but should work
on all cases that were described on piazza*/
void seed(char* request, struct sockaddr_in* client, GHashTable* strain, const char type){
	gchar** strings = g_strsplit(request, "\n", -1);
	uint i = 1;
	gchar** t = g_strsplit(strings[0], " ", -1);
	gchar** k = g_strsplit(t[1], "?", -1);
	/*If there is at least one question mark in the query string, then we know we either have a request to change
	the color or a string with variables. If the query is: "/color?bg=red" for example, then we have a request which 
	displays a empty html only with the background set and also sends a set-cookie header field to the client. If not, then
	the function expects this to be a query string like: "/asdf?arg1=val1&arg2=val2", which is parsed as seen below.*/
	if(g_strv_length(k) > 1){
		if(g_strcmp0(k[0] + 1, "color") == 0){
			g_hash_table_insert(strain, g_strdup("Color"), g_strdup(k[1] + 3));
		}
		else{
			/*Each variable should be seperated by a "&"*/
			gchar** l = g_strsplit(k[1], "&", -1);
			uint x = 0;
			gchar* n;
			gchar** s;
			int it = 0;
			/*This loop takes all argument strings which should be of the form: "arg=value" and allocates memory for them so
			they can be inserted into the hash table, each argument needs two allocations since we don't know the name of the argument.
			If a argument does not have a value, we skip it*/
			while(x < g_strv_length(l)){
				s = g_strsplit(l[x], "=", -1);
				if(g_strv_length(s) < 2){
					x++;
					continue;
				}
				it++;
				n = (gchar*)g_malloc(10);
				g_snprintf(n, 10, "arg%u%c", x, '\0');
				g_hash_table_insert(strain, n, g_strdup(s[0]));
				n = (gchar*)g_malloc(10);
				g_snprintf(n, 10, "val%u%c", x, '\0');
				g_hash_table_insert(strain, n, g_strdup(s[1]));
				g_strfreev(s);
				x++;
			}
			/*If there was at least one argument that was legal then we put the number of arguments into the hash table*/
			if(it > 0){
				n = (gchar*)g_malloc(10);
				g_snprintf(n, 10, "%u", it);
				g_hash_table_insert(strain, g_strdup("NArgs"), n);
			}
			/*The last thing we do is put the whole query string that we just parsed into the hash table*/
			gchar* q = g_strdup(t[1]);
			g_hash_table_insert(strain, g_strdup("Query"), q);
			g_strfreev(l);
		}
	}
	/*If those two cases mentioned above are not satisfied then we treat this as a normal query string which
	is explained in the generatehtml function*/
	else{
		gchar* q = g_strdup(t[1]);
		g_hash_table_insert(strain, g_strdup("Query"), q);
	}
	g_strfreev(k);
	g_strfreev(t);
	/*Get all header fields, into the hash table as key and value pairs*/
	while(i < g_strv_length(strings) - 2){
		t = g_strsplit(strings[i], ":", 2);
		g_hash_table_insert(strain, g_strdup(t[0]), g_strdup(t[1] + 1));
		g_strfreev(t);
		i += 1;
	}
	/*Get the client ip address and port from the client struct, they need to be parsed into strings, ofcourse
	both inserted into the hash table*/
	uint ip = client->sin_addr.s_addr;
	gchar* a = (gchar*)g_malloc(INET_ADDRSTRLEN);
	inet_ntop(AF_INET, &ip, a, INET_ADDRSTRLEN);
	g_hash_table_insert(strain, g_strdup("Client-Address"), a);
	uint port = htons(client->sin_port);
	gchar* b = (gchar*)g_malloc(6);
	g_snprintf(b, 6, "%u%c", port, '\0');
	g_hash_table_insert(strain, g_strdup("Port"), b);
	/*If we have a post request, we must save that content into the hash table so it can be retrieved to display
	the request*/ 
	if(type == 'p'){
		g_hash_table_insert(strain, g_strdup("Post-Content"), g_strdup(strings[g_strv_length(strings) - 1]));
	}
	g_strfreev(strings);
}

/*This function returns a time stamp for the requested format, a pointer to a allocated string from the heap
is returned, it should be freed after use*/
char* timestamp(){
	time_t rawtime;
	struct tm* info;
	char buffer[40];
	rawtime = time(NULL);
	info = localtime(&rawtime);
	strftime(buffer, sizeof(buffer) - 1, "%a, %d %b %Y %I:%M:%S %Z", info);
	return g_strdup(buffer);
}

/*Function which logs the given request to a given parameter*/
void logtofile(FILE* logger, const char type, GHashTable* strain){
	char method[5];
	/*Depending on the request*/
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
	char* t = timestamp();
	g_fprintf(logger, "%s : %s:%s %s %s : %s\n", t, g_hash_table_lookup(strain, "Client-Address"), g_hash_table_lookup(strain, "Port"), method, g_hash_table_lookup(strain, "Query"), "200");
	g_free(t);
    fclose(logger);
}
