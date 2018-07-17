#include "bunker.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#define SERVER_STRING "Server: Bunker/0.1.0\r\n"

FILE *logp;
bool g_debug;

#define LOG(fmt, ...) \
	do { \
		if (g_debug) { \
			printf(fmt, __VA_ARGS__); \
			fflush(stdout); \
		} else { \
			fprintf(logp, fmt, __VA_ARGS__); \
			fflush(logp); \
		} \
	} while(false); \

void error(const char *s)
{
	perror(s);
	exit(EXIT_FAILURE);
}

int strcasecmp(const char *s1, const char *s2)
{
	if (strlen(s1) != strlen(s2))
		return strlen(s1) - strlen(s2);
	size_t i = 0;
	for (i = 0; i < strlen(s1) && tolower((int)s1[i]) == tolower((int)s2[i]); ++i) {}
	return tolower((int)s1[i]) - tolower((int)s2[i]);
}

/*
 * HTTP Request line end error("Socket Error\n");with '\r\n'.
 * So when we read '/r', '/n' or '/r/n', replace with a '\n' and add '\0'.
 */
size_t read_line(int sock, char *buffer, size_t size)
{
	size_t i = 0;
	char ch = '\0';
	// size - 1: 1 byte for '\0'.
	while ((i < size - 1) && (ch != '\n')) {
		if (recv(sock, &ch, 1, 0) > 0) {
			// If we meet '/r', we need to see whether next is '\n'.
			if (ch == '\r') {
				// Set MEG_PEEK will leave the read char in stream.
				ssize_t n = recv(sock, &ch, 1, MSG_PEEK);
				if ((n > 0) && (ch == '\n'))
					// If we got '\n', read it.
					recv(sock, &ch, 1, 0);
				else
					ch = '\n';
			}
			buffer[i++] = ch;
		} else {
			break;
		}
	}
	buffer[i] = '\0';
	return i;
}

void accept_request(int client, const char *dir)
{
	char buffer[1024];
	char method[256];
	char url[256];
	char path[512];
	bool cgi = false;
	struct stat state;
	char *query_string = NULL;
	size_t string_length = 0;
	unsigned short code = 200;

	string_length = read_line(client, buffer, sizeof(buffer));
	size_t i = 0;
	size_t j = 0;
	// Get method.
	//  sizeof(method) - 1: For '\0'.
	while (!isspace((int)buffer[i]) && j < sizeof(method) - 1)
		method[j++] = buffer[i++];
	method[j] = '\0';
#ifdef __DEBUG__
	LOG("Bunker[%d]: DEBUG: %s: %d: METHOD: %s\n", getpid(), __FILE__, __LINE__, method);
#endif
	// strcasecmp(): Case insensitive.
	if (strcasecmp(method, "GET") && strcasecmp(method, "POST")) {
		// TODO: Unimplement here.
		throw_unimplement_method(client, &code);
		return;
	}

	// Remove space.
	while (isspace((int)buffer[i]) && i < sizeof(buffer))
		++i;

	// Get URL.
	j = 0;
	while (!isspace((int)buffer[i]) && i < sizeof(buffer) && j < sizeof(url) - 1)
		url[j++] = buffer[i++];
	url[j] = '\0';
#ifdef __DEBUG__
	LOG("Bunker[%d]: DEBUG: %s: %d: URL: %s\n", getpid(), __FILE__, __LINE__, url);
#endif

	if (!strcasecmp(method, "GET")) {
		query_string = url;
		while (*query_string != '?' && *query_string != '\0')
			++query_string;
		if (*query_string == '?') {
			cgi = true;
			*query_string = '\0';
			++query_string;
#ifdef __DEBUG__
			LOG("Bunker[%d]: DEBUG: %s: %d: QUERYSTRING: %s\n", getpid(), __FILE__, __LINE__, query_string);
#endif
		}
	}

	snprintf(path, sizeof(path), "%s%s", dir, url);
#ifdef __DEBUG__
	LOG("Bunker[%d]: DEBUG: %s: %d: PATH: %s\n", getpid(), __FILE__, __LINE__, path);
#endif
	if (!strlen(path))
		strncat(path, "/", sizeof(path) - strlen(path) - 1);
	if (path[strlen(path) - 1] == '/')
		strncat(path, "index.html", sizeof(path) - strlen(path) - 1);
	if (stat(path, &state) == -1) {
		// If not found we need to read all of the request then ignore.
		while (string_length > 0 && strcmp("\n", buffer))
			string_length = read_line(client, buffer, sizeof(buffer));
		throw_not_found(client, &code);
	} else {
		// If file is still a dir.
	        if (S_ISDIR(state.st_mode))
			strncat(path, "/index.html", sizeof(path) - strlen(path) - 1);
#ifdef __DEBUG__
	LOG("Bunker[%d]: DEBUG: %s: %d: PATH: %s\n", getpid(), __FILE__, __LINE__, path);
#endif
		// Executable.
		if (!access(path, X_OK))
		    cgi = true;
		if (!cgi) {
			// Ignore headers.
			string_length = 1;
			while (string_length > 0 && strcmp("\n", buffer))
				string_length = read_line(client, buffer, sizeof(buffer));
#ifdef __DEBUG__
			LOG("Bunker[%d]: DEBUG: %s: %d: %s\n", getpid(), __FILE__, __LINE__, path);
#endif
			send_file(client, &code, path);
		} else {
#ifdef __DEBUG__
			LOG("Bunker[%d]: DEBUG: %s: %d: CGI\n", getpid(), __FILE__, __LINE__);
#endif
			execute_cgi(client, &code, path, method, query_string);
		}
		close(client);
		LOG("Bunker[%d]: %hu: Method: '%s'. Path: '%s'. Query string: '%s'. CGI mode: '%s'.\n", getpid(), code, method, path, query_string ? query_string : "NULL", cgi ? "true" : "false");
	}
}

void send_file(int client, unsigned short *codep, const char *path)
{
	if (access(path, R_OK)) {
		throw_forbidden(client, codep);
		return;
	}
	send_headers(client, path);
	cat(client, path);
}

void send_headers(int client, const char *path)
{
	char buffer[1024];
	// TODO: Content-Type detect, Content_Length detect un implemented here.
	strncpy(buffer, "HTTP/1.0 200 OK\r\n", sizeof(buffer));
	send(client, buffer, strlen(buffer), 0);
	strncpy(buffer, SERVER_STRING, sizeof(buffer));
	send(client, buffer, strlen(buffer), 0);
	strncpy(buffer, "Content-Type: text/html\r\n", sizeof(buffer));
	send(client, buffer, strlen(buffer), 0);
	strncpy(buffer, "\r\n", sizeof(buffer));
	send(client, buffer, strlen(buffer), 0);
}

void cat(int client, const char *path)
{
	FILE *fp = NULL;
	if (!(fp = fopen(path, "r"))) {
		return;
	}
	char buffer[1024];
	while(fgets(buffer, sizeof(buffer), fp))
		send(client, buffer, strlen(buffer), 0);
	fclose(fp);
}

void execute_cgi(int client, unsigned short *codep, const char *path, const char *method, const char *query_string)
{
	char buffer[1024] = "";
	int cgi_output[2];
	int cgi_input[2];
	int status;
	size_t string_length = 1;
	size_t content_length = -1;
	if (!strcasecmp(method, "GET")) {
		while (string_length > 0 && strcmp("\n", buffer))
			string_length = read_line(client, buffer, sizeof(buffer));
	} else if (!strcasecmp(method, "POST")) {
		string_length = read_line(client, buffer, sizeof(buffer));
		while (string_length > 0 && strcmp("\n", buffer)) {
	        	buffer[strlen("Content-Length:")] = '\0';
	        	if (strcasecmp(buffer, "Content-Length:") == 0)
	        		content_length = strtol(&buffer[16], NULL, 10);
		        string_length = read_line(client, buffer, sizeof(buffer));
	        }
		if (content_length == -1) {
	         	throw_bad_request(client, codep);
	         	return;
	        }
	}

	strncpy(buffer, "HTTP/1.0 200 OK\r\n", sizeof(buffer));
	send(client, buffer, strlen(buffer), 0);
	strncpy(buffer, "Content-Type: text/plain\r\n", sizeof(buffer));
	strncpy(buffer, "\r\n", sizeof(buffer));
	send(client, buffer, strlen(buffer), 0);

	// Pipe for ipc.
	if (pipe(cgi_output) < 0) {
        	throw_internal_server_error(client, codep);
        	return;
        }
        if (pipe(cgi_input) < 0) {
		throw_internal_server_error(client, codep);
        	return;
        }
	pid_t pid = fork();
	if (pid < 0) {
		// Fork error.
		throw_internal_server_error(client, codep);
		return;
	} else if (pid > 0) {
		// Parent process
		close(cgi_output[1]);
	        close(cgi_input[0]);
	        if (!strcasecmp(method, "POST")) {
			// Read body.
			char ch = '\0';
	        	for (size_t i = 0; i < content_length; ++i) {
	        		recv(client, &ch, 1, 0);
	        		write(cgi_input[1], &ch, 1);
	        	}
		}
		char ch = '\0';
	        while (read(cgi_output[0], &ch, 1) > 0)
			send(client, &ch, 1, 0);
	        close(cgi_output[0]);
	        close(cgi_input[1]);
	        waitpid(pid, &status, 0);
	} else {
		// Child process.
		/*
		 * cgi_output[0]: read.
		 * cgi_output[1]: write.
		 * cgi_input[0]: read.
		 * cgi_input[1]: write.
		 */
		dup2(cgi_output[1], 1);
		dup2(cgi_input[0], 0);
		close(cgi_output[0]);
	        close(cgi_input[1]);
		if (!strcasecmp(method, "GET")) {
			execl(path, path, method, query_string, NULL);
	        } else if (!strcasecmp(method, "POST")) {
			char buffer[1024];
			snprintf(buffer, sizeof(buffer), "%lu", content_length);
			execl(path, path, method, buffer, NULL);
	        }
		exit(EXIT_SUCCESS);
	}
}

void throw_unimplement_method(int client, unsigned short *codep)
{
	*codep = 501;
	char buffer[1024];
	strncpy(buffer, "HTTP/1.0 501 Method Not Implemented\r\n", sizeof(buffer));
	send(client, buffer, strlen(buffer), 0);
	strncpy(buffer, SERVER_STRING, sizeof(buffer));
	send(client, buffer, strlen(buffer), 0);
	strncpy(buffer, "Content-Type: text/html\r\n", sizeof(buffer));
	send(client, buffer, strlen(buffer), 0);
	strncpy(buffer, "\r\n", sizeof(buffer));
	send(client, buffer, strlen(buffer), 0);
	strncpy(buffer, "<html><head><title>501 Method Not Implemented</title></head>", sizeof(buffer));
	send(client, buffer, strlen(buffer), 0);
	strncpy(buffer, "<body><h1>501 Method Not Implemented</h1></body></html>", sizeof(buffer));
	send(client, buffer, strlen(buffer), 0);
}

void throw_internal_server_error(int client, unsigned short *codep)
{
	*codep = 500;
	char buffer[1024];
	strncpy(buffer, "HTTP/1.0 500 Internal Server Error\r\n", sizeof(buffer));
	send(client, buffer, strlen(buffer), 0);
	strncpy(buffer, SERVER_STRING, sizeof(buffer));
	send(client, buffer, strlen(buffer), 0);
	strncpy(buffer, "Content-Type: text/html\r\n", sizeof(buffer));
	send(client, buffer, strlen(buffer), 0);
	strncpy(buffer, "\r\n", sizeof(buffer));
	send(client, buffer, strlen(buffer), 0);
	strncpy(buffer, "<html><head><title>500 Internal Server Error</title></head>", sizeof(buffer));
	send(client, buffer, strlen(buffer), 0);
	strncpy(buffer, "<body><h1>500 Internal Server Error</h1></body></html>", sizeof(buffer));
	send(client, buffer, strlen(buffer), 0);
}

void throw_not_found(int client, unsigned short *codep)
{
	*codep = 404;
	char buffer[1024];
	strncpy(buffer, "HTTP/1.0 404 Not Found\r\n", sizeof(buffer));
	send(client, buffer, strlen(buffer), 0);
	strncpy(buffer, SERVER_STRING, sizeof(buffer));
	send(client, buffer, strlen(buffer), 0);
	strncpy(buffer, "Content-Type: text/html\r\n", sizeof(buffer));
	send(client, buffer, strlen(buffer), 0);
	strncpy(buffer, "\r\n", sizeof(buffer));
	send(client, buffer, strlen(buffer), 0);
	strncpy(buffer, "<html><head><title>404 Not Found</title></head>", sizeof(buffer));
	send(client, buffer, strlen(buffer), 0);
	strncpy(buffer, "<body><h1>404 Not Found</h1></body></html>", sizeof(buffer));
	send(client, buffer, strlen(buffer), 0);
}

void throw_forbidden(int client, unsigned short *codep)
{
	*codep = 403;
	char buffer[1024];
	strncpy(buffer, "HTTP/1.0 403 Forbidden\r\n", sizeof(buffer));
	send(client, buffer, strlen(buffer), 0);
	strncpy(buffer, SERVER_STRING, sizeof(buffer));
	send(client, buffer, strlen(buffer), 0);
	strncpy(buffer, "Content-Type: text/html\r\n", sizeof(buffer));
	send(client, buffer, strlen(buffer), 0);
	strncpy(buffer, "\r\n", sizeof(buffer));
	send(client, buffer, strlen(buffer), 0);
	strncpy(buffer, "<html><head><title>403 Forbidden</title></head>", sizeof(buffer));
	send(client, buffer, strlen(buffer), 0);
	strncpy(buffer, "<body><h1>403 Forbidden</h1></body></html>", sizeof(buffer));
	send(client, buffer, strlen(buffer), 0);
}

void throw_bad_request(int client, unsigned short *codep)
{
	*codep = 400;
	char buffer[1024];
	strncpy(buffer, "HTTP/1.0 400 Bad Request\r\n", sizeof(buffer));
	send(client, buffer, strlen(buffer), 0);
	strncpy(buffer, SERVER_STRING, sizeof(buffer));
	send(client, buffer, strlen(buffer), 0);
	strncpy(buffer, "Content-Type: text/html\r\n", sizeof(buffer));
	send(client, buffer, strlen(buffer), 0);
	strncpy(buffer, "\r\n", sizeof(buffer));
	send(client, buffer, strlen(buffer), 0);
	strncpy(buffer, "<html><head><title>400 Bad Request</title></head>", sizeof(buffer));
	send(client, buffer, strlen(buffer), 0);
	strncpy(buffer, "<body><h1>400 Bad Request</h1></body></html>", sizeof(buffer));
	send(client, buffer, strlen(buffer), 0);
}

int start_server(const char *address, unsigned short port, const char *dir, const char *log_file, bool debug)
{
	int httpd = 0;
	g_debug = debug;
	if (!g_debug)
		if (!(logp = fopen(log_file, "a+")))
			perror("Log Error");
	int af = AF_INET;
	af = AF_INET;
	if (address && strchr(address, ':'))
		af = AF_INET6;
	if (af == AF_INET) {
		struct sockaddr_in name;
		memset(&name, 0, sizeof(name));
		name.sin_family = af;
		name.sin_port = htons(port);
		if (!address || !inet_pton(af, address, &(name.sin_addr)))
			name.sin_addr.s_addr = htonl(INADDR_ANY);
		if ((httpd = socket(af, SOCK_STREAM, 0)) == -1)
			error("Socket Error");
		const int ON = 1;
		if (setsockopt(httpd, SOL_SOCKET, SO_REUSEADDR, &ON, sizeof(ON)) < 0)
			error("Setsocketopt Error");
		if (bind(httpd, (struct sockaddr *)&name, sizeof(name)) < 0)
			error("Bind Error");
	} else if (af == AF_INET6) {
		struct sockaddr_in6 name;
		memset(&name, 0, sizeof(name));
		name.sin6_family = af;
		name.sin6_port = htons(port);
		if (!address || !inet_pton(af, address, &(name.sin6_addr)))
			name.sin6_addr = in6addr_any;
		if ((httpd = socket(af, SOCK_STREAM, 0)) == -1)
			error("Socket Error");
		const int ON = 1;
		if (setsockopt(httpd, SOL_SOCKET, SO_REUSEADDR, &ON, sizeof(ON)) < 0)
			error("Setsocketopt Error");
		if (bind(httpd, (struct sockaddr *)&name, sizeof(name)) < 0)
			error("Bind Error");
	}
	if (listen(httpd, 5) < 0)
		error("Listen Error");
	LOG("Bunker[%d]: Bunker is listening at '%s:%hu' ...\n", getpid(), address ? address : "0.0.0.0", port);
	LOG("Bunker[%d]: Log file: '%s'. Doc dir: '%s'. Debug mode: '%s'.\n", getpid(), log_file, dir, debug ? "true" : "false");
	return httpd;
}

void stop_server(int server)
{
	fclose(logp);
	close(server);
}
