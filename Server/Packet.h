#pragma once

//
//		Клиент-серверный протокол обмена данными
//		Packet version 2.0.2
//		22.07.2017
//

#define PACKET_SIZE	4112					// Размер сообщения, которым обмениваются сервер и клиент
#define PACKET_DATA_SIZE 4096				// Размер данных в пакете

// Сообщения, которые принимает и посылает сервер
#define PACKET_ADD_ID				2001 // Команда запрашивает у клиента информацию о системе
#define PACKET_USER_JOINED			2002 // Ответ клиента с информацией
#define PACKET_CMD					2003 // Команда клиенту - выполнить командную строку
#define PACKET_CMD_REPORT			2004 // Отчет клиента о выполненной командной строке
#define PACKET_TEST					2005 // Запрос клиента протестировать соединение
#define PACKET_TEST_ECHO			2006 // Ответ сервера на тест соединения
#define PACKET_CLOSE_CLIENT			2007 // Сервер шлет своему окну сообщение о отсоединении клиента

// Сообщения файлового менеджера
#define	 PACKET_FILE_MANAGER_DRIVE				2010
#define  PACKET_FILE_MANAGER_FILE				2011
#define  PACKET_FILE_MANAGER_NEW_FILE			2012

struct Packet
{
	DWORD type;
	DWORD id;
	DWORD id1;
	DWORD id2;
	char data[PACKET_DATA_SIZE];
};

