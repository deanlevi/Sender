#include <stdio.h>

#include "Sender.h"

#define SUCCESS_CODE 0

int main(int argc, char *argv[]) {
	if (argc != 4) {
		fprintf(stderr, "Not the right amount of input arguments.\nNeed to give two.\nExiting...\n"); // first is path, other three are inputs
		return ERROR_CODE;
	}
	InitSender(argv);
	HandleSendFile();
	HandleReceiveFromChannel();
	CloseSocketsAndWsaData();
	return SUCCESS_CODE;
}