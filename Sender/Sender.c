#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS

#include <stdlib.h>
#include <stdio.h>

#include "Sender.h"

#define CHANNEL_IP_ADDRESS_ARGUMENT_INDEX 1
#define CHANNEL_PORT_NUM_ARGUMENT_INDEX 2
#define INPUT_FILE_TO_TRANSFER_ARGUMENT_INDEX 3
#define SOCKET_PROTOCOL 0
#define INPUT_CHUNK_SIZE 7 // 7 bytes = 56 bits
#define OUTPUT_CHUNK_SIZE 8 // 8 bytes = 64 bits
#define NUM_OF_SPARE_BITS_FROM_EACH_CHUNK 7 // 56 - 49 = 7
#define NUM_OF_DATA_BITS_IN_ONE_CHUNK 49
#define NUM_OF_ERROR_BITS 15
#define NUM_OF_BITS_IN_A_ROW_COLUMN 7
#define SEND_RECEIVE_FLAGS 0
#define MESSAGE_LENGTH 20
#define SEND_MESSAGES_WAIT 20


/*
Input: argv - to update input parameters.
Output: none.
Description: update sender parameters and init variables.
*/
void InitSender(char *argv[]);

/*
Input: none.
Output: none.
Description: open and reading file to send in chunks of 7 bytes. in each chunk, take 49 bits and save 15 bits for next chunk.
			 calculate parity for 49 bits and put it in msb of sent chunk. sent chunk is 64 bits = 8 bytes.
*/
void HandleSendFile();

/*
Input: DataToSend - coded chunk of data to send to channel (49 bits of data + 15 error bits = 64 bits).
Output: none.
Description: send DataToSend to channel.
*/
void SendData(unsigned long long DataToSend);

/*
Input: DataToSend - data to send to receiver before addition of error bits.
Output: none.
Description: creation of 15 error bits and adding them to msb of DataToSend.
			 the mapping of each bit is as follows:
			 bit 0 of error bits = bit 49 of DataToSend = parity bit of bits 0-6 in DataToSend (row).
			 bit 1 of error bits = bit 50 of DataToSend = parity bit of bits 7-13 in DataToSend (row).
			 bit 2 of error bits = bit 51 of DataToSend = parity bit of bits 14-20 in DataToSend (row).
			 bit 3 of error bits = bit 52 of DataToSend = parity bit of bits 21-27 in DataToSend (row).
			 bit 4 of error bits = bit 53 of DataToSend = parity bit of bits 28-34 in DataToSend (row).
			 bit 5 of error bits = bit 54 of DataToSend = parity bit of bits 35-41 in DataToSend (row).
			 bit 6 of error bits = bit 55 of DataToSend = parity bit of bits 42-49 in DataToSend (row).
			 bit 7 of error bits = bit 56 of DataToSend = parity bit of bits  0,7,14,21,28,35,42 in DataToSend (column).
			 bit 8 of error bits = bit 57 of DataToSend = parity bit of bits  1,8,15,22,29,36,43 in DataToSend (column).
			 bit 9 of error bits = bit 58 of DataToSend = parity bit of bits  2,9,16,23,30,37,44 in DataToSend (column).
			 bit 10 of error bits = bit 59 of DataToSend = parity bit of bits 3,10,17,24,31,38,45 in DataToSend (column).
			 bit 11 of error bits = bit 60 of DataToSend = parity bit of bits 4,11,18,25,32,39,46 in DataToSend (column).
			 bit 12 of error bits = bit 61 of DataToSend = parity bit of bits 5,12,19,26,33,40,47 in DataToSend (column).
			 bit 13 of error bits = bit 62 of DataToSend = parity bit of bits 6,13,20,27,34,41,48 in DataToSend (column).
			 bit 14 of error bits = bit 63 of DataToSend = parity bit of bits 7-13 of error bits.

*/
void AddErrorFixingBits(unsigned long long *DataToSend);

/*
Input: none.
Output: none.
Description: handle received data from receiver and outputing information.
*/
void HandleReceiveFromChannel();

/*
Input: ParameterToUpdate - pointer to the parameter to be updated, StartIndex - current start index in received message from receiver.
						   EndIndex - current end index in received message from receiver, MessageFromChannel - whole message from receiver.
Output: none.
Description: parse one parameter in received data from receiver.
*/
void ParseParameter(int *ParameterToUpdate, int *StartIndex, int *EndIndex, char MessageFromChannel[MESSAGE_LENGTH]);

/*
Input: none.
Output: none.
Description: close sockets and wsa data at the end of the sender's operation.
*/
void CloseSocketsAndWsaData();

void InitSender(char *argv[]) {
	Sender.ChannelIPAddress = argv[CHANNEL_IP_ADDRESS_ARGUMENT_INDEX];
	Sender.ChannelPortNum = atoi(argv[CHANNEL_PORT_NUM_ARGUMENT_INDEX]);
	Sender.InputFileToTransfer = argv[INPUT_FILE_TO_TRANSFER_ARGUMENT_INDEX];
	Sender.ChannelSocketService.sin_family = AF_INET;
	Sender.ChannelSocketService.sin_addr.s_addr = inet_addr(Sender.ChannelIPAddress);
	Sender.ChannelSocketService.sin_port = htons(Sender.ChannelPortNum);

	WSADATA wsaData;
	int StartupRes = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (StartupRes != NO_ERROR) {
		fprintf(stderr, "Error %ld at WSAStartup().\nExiting...\n", StartupRes);
		exit(ERROR_CODE);
	}

	Sender.ChannelSocket = socket(AF_INET, SOCK_DGRAM, SOCKET_PROTOCOL);
	if (Sender.ChannelSocket == INVALID_SOCKET) {
		fprintf(stderr, "InitSender failed to create socket. Error Number is %d\n", WSAGetLastError());
		CloseSocketsAndWsaData();
		exit(ERROR_CODE);
	}
}

void HandleSendFile() {
	FILE *InputFilePointer = fopen(Sender.InputFileToTransfer, "r");
	if (InputFilePointer == NULL) {
		fprintf(stderr, "HandleSendFile couldn't open input file.\n");
		CloseSocketsAndWsaData();
		exit(ERROR_CODE);
	}
	unsigned long long InputChunk = 0, DataToSend = 0, DataToSendNext = 0, TempData = 0;
	int DataToSendNextOffset = 0;
	size_t ReadElements;
	Sleep(SEND_MESSAGES_WAIT); // if starting all at the same time

	while (TRUE) {
		if (DataToSendNextOffset < NUM_OF_DATA_BITS_IN_ONE_CHUNK) {
			ReadElements = fread(&InputChunk, INPUT_CHUNK_SIZE, 1, InputFilePointer);
			if (ReadElements != 1) { // reached EOF
				break;
			}
			DataToSend = DataToSendNext << (NUM_OF_DATA_BITS_IN_ONE_CHUNK - DataToSendNextOffset);
			DataToSendNextOffset = DataToSendNextOffset + NUM_OF_SPARE_BITS_FROM_EACH_CHUNK;
			TempData = InputChunk >> DataToSendNextOffset;
			DataToSend += TempData;
			DataToSendNext = InputChunk - (TempData << DataToSendNextOffset);
		}
		else {
			DataToSend = DataToSendNext;
			DataToSendNext = 0;
			DataToSendNextOffset = 0;
		}
		AddErrorFixingBits(&DataToSend);
		SendData(DataToSend);
		InputChunk = 0;
	}
	fclose(InputFilePointer);
}

void SendData(unsigned long long DataToSend) {
	int SentBufferLength = sendto(Sender.ChannelSocket, &DataToSend, OUTPUT_CHUNK_SIZE, SEND_RECEIVE_FLAGS,
		(SOCKADDR*)&Sender.ChannelSocketService, sizeof(Sender.ChannelSocketService));
	if (SentBufferLength == SOCKET_ERROR) {
		fprintf(stderr, "SendData failed to sendto. Error Number is %d\n", WSAGetLastError());
		CloseSocketsAndWsaData();
		exit(ERROR_CODE);
	}
	Sleep(SEND_MESSAGES_WAIT);
}

void AddErrorFixingBits(unsigned long long *DataToSend) {
	unsigned long long ErrorBits = 0;
	int RowParity = 0, ColumnParity = 0, DiagonalParity = 0, RowParityPositionInDataToSend, ColumnParityPositionInDataToSend,
		RowBit, ColumnBit;

	for (int RowIndexInDataToSend = 0; RowIndexInDataToSend < NUM_OF_BITS_IN_A_ROW_COLUMN; RowIndexInDataToSend++) {
		for (int ColumnIndexInDataToSend = 0; ColumnIndexInDataToSend < NUM_OF_BITS_IN_A_ROW_COLUMN; ColumnIndexInDataToSend++) {
			RowParityPositionInDataToSend = ColumnIndexInDataToSend + NUM_OF_BITS_IN_A_ROW_COLUMN * RowIndexInDataToSend;
			RowBit = (*DataToSend >> RowParityPositionInDataToSend) & 1;
			RowParity = RowParity ^ RowBit;
			ColumnParityPositionInDataToSend = RowIndexInDataToSend + NUM_OF_BITS_IN_A_ROW_COLUMN * ColumnIndexInDataToSend;
			ColumnBit = (*DataToSend >> ColumnParityPositionInDataToSend) & 1;
			ColumnParity = ColumnParity ^ ColumnBit;
		}
		ErrorBits = ErrorBits + (RowParity << RowIndexInDataToSend);
		ErrorBits = ErrorBits + (ColumnParity << (RowIndexInDataToSend + NUM_OF_BITS_IN_A_ROW_COLUMN));
		DiagonalParity = DiagonalParity ^ ColumnParity;
		RowParity = 0;
		ColumnParity = 0;
	}
	ErrorBits = ErrorBits + (DiagonalParity << (NUM_OF_ERROR_BITS - 1)); // ErrorBits are 15 bits
	ErrorBits = ErrorBits << (NUM_OF_BITS_IN_A_ROW_COLUMN*NUM_OF_BITS_IN_A_ROW_COLUMN); // shift 49 to the left
	*DataToSend = *DataToSend + ErrorBits; // make error bits msb of data
}

void HandleReceiveFromChannel() {
	char MessageFromChannel[MESSAGE_LENGTH];
	int ReceivedBufferLength;

	ReceivedBufferLength = recvfrom(Sender.ChannelSocket, MessageFromChannel, MESSAGE_LENGTH, SEND_RECEIVE_FLAGS, NULL, NULL);
	if (ReceivedBufferLength == SOCKET_ERROR) {
		fprintf(stderr, "HandleReceiveFromChannel failed to recvfrom. Error Number is %d\n", WSAGetLastError());
		CloseSocketsAndWsaData();
		exit(ERROR_CODE);
	}

	int NumberOfReceivedBytes, NumberOfWrittenBytes, NumberOfErrorsDetected, NumberOfErrorsCorrected;
	int StartIndex = 0, EndIndex = 0;

	ParseParameter(&NumberOfReceivedBytes, &StartIndex, &EndIndex, MessageFromChannel);
	ParseParameter(&NumberOfWrittenBytes, &StartIndex, &EndIndex, MessageFromChannel);
	ParseParameter(&NumberOfErrorsDetected, &StartIndex, &EndIndex, MessageFromChannel);
	ParseParameter(&NumberOfErrorsCorrected, &StartIndex, &EndIndex, MessageFromChannel);

	fprintf(stderr, "received: %d bytes\n", NumberOfReceivedBytes);
	fprintf(stderr, "wrote: %d bytes\n", NumberOfWrittenBytes);
	fprintf(stderr, "detected: %d errors, corrected: %d errors\n", NumberOfErrorsDetected, NumberOfErrorsCorrected);
}

void ParseParameter(int *ParameterToUpdate, int *StartIndex, int *EndIndex, char MessageFromChannel[MESSAGE_LENGTH]) {
	char ParameterAsString[MESSAGE_LENGTH];
	while (MessageFromChannel[*EndIndex] != '\n') {
		(*EndIndex)++;
	}
	strncpy(ParameterAsString, MessageFromChannel + *StartIndex, *EndIndex - *StartIndex);
	*ParameterToUpdate = atoi(ParameterAsString);
	(*EndIndex)++;
	*StartIndex = *EndIndex;
}

void CloseSocketsAndWsaData() {
	int CloseSocketReturnValue;
	CloseSocketReturnValue = closesocket(Sender.ChannelSocket);
	if (CloseSocketReturnValue == SOCKET_ERROR) {
		fprintf(stderr, "CloseSocketsAndWsaData failed to close socket. Error Number is %d\n", WSAGetLastError());
		exit(ERROR_CODE);
	}
	if (WSACleanup() == SOCKET_ERROR) {
		fprintf(stderr, "CloseSocketsAndWsaData Failed to close Winsocket, error %ld. Ending program.\n", WSAGetLastError());
		exit(ERROR_CODE);
	}
}