#pragma once

/* main.cpp about receiving and sending data through a socket, replaced by player.h
	std::function<int(SOCKET)> recvJob = [](SOCKET cSocket) -> int
	{
		char recvbuf[NET_PACK_MAX_LEN];
		int iResult, iSendResult;
		int recvbuflen = NET_PACK_MAX_LEN;
		// Receive until the peer shuts down the connection
		do {
			iResult = recv(cSocket, recvbuf, recvbuflen, 0);
			if (iResult > 0) {

				const std::string serverRespond = "Server Receives Success";
				iSendResult = send(cSocket, serverRespond.c_str(), serverRespond.length() + 1, 0);
				if (iSendResult == SOCKET_ERROR) {
					printf("send failed: %d\n", WSAGetLastError());
					closesocket(cSocket);
					WSACleanup();
					return 1;
				}
			}
			else if (iResult == 0) {
				printf("Connection closing...\n");
			}
			else {
				printf("recv failed: %d\n", WSAGetLastError());
				closesocket(cSocket);
				WSACleanup();
				return 1;
			}
		} while (iResult > 0);
		// shutdown the send half of the connection since no more data will be sent
		iResult = shutdown(cSocket, SD_SEND);
		if (iResult == SOCKET_ERROR) {
			printf("shutdown failed: %d\n", WSAGetLastError());
			closesocket(cSocket);
			WSACleanup();
			return 1;
		}
		closesocket(cSocket);
		return 0;
	};
*/