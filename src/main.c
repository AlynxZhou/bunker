#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include "bunker.h"
#include "getarg/getarg.h"

int server_sock = -1;
int client_sock = -1;

static void sig_handler(int sig)
{
	close(client_sock);
	stop_server(server_sock);
	exit(EXIT_SUCCESS);
}

void print_help(void);

int main(int argc, char *argv[])
{
	signal(SIGINT, sig_handler);
	signal(SIGTERM, sig_handler);
	const char OPT_STRING[] = "hdc:i:l:p:";
	const char *address = NULL;
	unsigned short port = 3333;
	const char *dir = ".";
	const char *log_file = "bunker.log";
	struct sockaddr_in client_name;
	socklen_t client_name_length = sizeof(client_name);
	bool debug = false;
	int opt = -1;
	while ((opt = getarg(argc, (const char *const *)argv,
			     OPT_STRING)) != -1) {
		switch (opt) {
		case 'h':
			print_help();
			exit(EXIT_SUCCESS);
			break;
		case 'd':
			debug = true;
			break;
		case 'c':
			dir = argopt;
			break;
		case 'i':
			address = argopt;
			break;
		case 'l':
			log_file = argopt;
			break;
		case 'p':
			sscanf(argopt, "%hu", &port);
			break;
		case 0:
			fprintf(stderr, "%s: Invalid value '%s'.\n", \
				argv[0], argopt);
			exit(EXIT_FAILURE);
			break;
		default:
			fprintf(stderr, "%s: Invalid option '%c%c'.\n", \
				argv[0], OPT_START, opt);
			exit(EXIT_FAILURE);
			break;
		}
    	}
	if (!debug) {
		pid_t pid = fork();
		if (pid > 0)
			exit(EXIT_SUCCESS);
		else if (pid < 0)
			error("Fork Error");
		setsid();
		// Change to work dir.
		chdir(dir);
		dir = ".";
		umask(0);
		fclose(stdin);
		fclose(stdout);
		fclose(stderr);
	}
	server_sock = start_server(address, port, dir, log_file, debug);
	while (true) {
		client_sock = accept(server_sock,
				     (struct sockaddr *)&client_name,
				     &client_name_length);
		if (client_sock == -1)
			error("Accept Error");
		accept_request(client_sock, dir);
	}
	stop_server(server_sock);
	return 0;
}

void print_help(void)
{
	printf("Bunker: A simple HTTP server.\n");
	printf("Written by AlynxZhou. Version 0.1.0.\n");
	printf("Usage: bunker [OPTION...] <value>\n");
	printf("Options:\n");
	printf("\t%ch\t\tDisplay this help.\n", OPT_START);
	printf("\t%cd\t\tRun in debug mode (NOT daemonize).\n", OPT_START);
	printf("\t%cc <dir>\tSet doc dir.\n", OPT_START);
	printf("\t%ci <address>\tSet listening address.\n", OPT_START);
	printf("\t%cl <path>\tSet log file path.\n", OPT_START);
	printf("\t%cp <port>\tSet listening port.\n", OPT_START);
}
