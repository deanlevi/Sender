#ifndef SENDER_H
#define SENDER_H

#include <WinSock2.h>

#pragma comment(lib, "Ws2_32.lib")

#define ERROR_CODE (int)(-1)

typedef struct _SenderProperties {
	char *ChannelIPAddress;
	int ChannelPortNum;
	char *InputFileToTransfer;

	SOCKET ChannelSocket;
	SOCKADDR_IN ChannelSocketService;
}SenderProperties;

SenderProperties Sender;

void InitSender(char *argv[]);
void HandleSendFile();
void HandleReceiveFromChannel();
void CloseSocketsAndWsaData();

#endif