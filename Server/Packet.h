#pragma once

//
//		������-��������� �������� ������ �������
//		Packet version 2.0.2
//		22.07.2017
//

#define PACKET_SIZE	4112					// ������ ���������, ������� ������������ ������ � ������
#define PACKET_DATA_SIZE 4096				// ������ ������ � ������

// ���������, ������� ��������� � �������� ������
#define PACKET_ADD_ID				2001 // ������� ����������� � ������� ���������� � �������
#define PACKET_USER_JOINED			2002 // ����� ������� � �����������
#define PACKET_CMD					2003 // ������� ������� - ��������� ��������� ������
#define PACKET_CMD_REPORT			2004 // ����� ������� � ����������� ��������� ������

struct Packet
{
	DWORD type;
	DWORD id;
	DWORD id1;
	DWORD id2;
	char data[PACKET_DATA_SIZE];
};

