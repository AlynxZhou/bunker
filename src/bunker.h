#ifndef __BUNKER_H__
#	define __BUNKER_H__

#	include <stddef.h>
#	include <stdbool.h>

void error(const char *s);
int strcasecmp(const char *s1, const char *s2);
size_t read_line(int sock, char *buffer, size_t size);
void accept_request(int client, const char *dir);
void execute_cgi(int client, unsigned short *codep, const char *path,
		 const char *method, const char *query_string);
void send_file(int client, unsigned short *codep, const char *path);
void send_headers(int client, const char *path);
void cat(int client, const char *path);
void throw_unimplement_method(int client, unsigned short *codep);
void throw_internal_server_error(int client, unsigned short *codep);
void throw_not_found(int client, unsigned short *codep);
void throw_forbidden(int client, unsigned short *codep);
void throw_bad_request(int client, unsigned short *codep);
int start_server(const char *address, unsigned short port, const char *dir,
		 const char *log_file, bool debug);
void stop_server(int server);

#endif
