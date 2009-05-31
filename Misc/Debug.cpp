#include "Debug.h"

bool Debug::bLog = true;
FILE* Debug::pLogFile = NULL;

void (_cdecl* Debug::Log)(const char* pFormat, ...) =
	(void (__cdecl *)(const char *,...))0x4068E0;

void Debug::LogFileOpen()
{
	LogFileClose();
	pLogFile = fopen(DEBUG_FILE, "w");
}

void Debug::LogFileClose()
{
	if(pLogFile)
		fclose(pLogFile);

	pLogFile = NULL;
}

void Debug::LogFileRemove()
{
	LogFileClose();
	remove(DEBUG_FILE);
}

void Debug::DumpObj(byte *data, size_t len) {
	Debug::Log("Dumping %u bytes of object at %X\n", len, data);

	Debug::Log(" 00000 |");
	for(DWORD rem = 0; rem < 0x10; ++rem) {
		Debug::Log(" %02X |", rem);
	}
	Debug::Log("\n\n");
	for(DWORD dec = 0; dec < len / 0x10; ++dec) {
		Debug::Log(" %04X0 |", dec);
		for(DWORD rem = 0; rem < 0x10; ++rem) {
			Debug::Log(" %02X |", data[dec * 0x10 + rem]);
		}
		for(DWORD rem = 0; rem < 0x10; ++rem) {
			byte sym = data[dec * 0x10 + rem];
			Debug::Log("%c", isprint(sym) ? sym : '?');
		}
		Debug::Log("\n");
	}
	Debug::Log("\n");
}

void Debug::DumpStack(REGISTERS *R, size_t len) {
	Debug::Log("Dumping %X bytes of stack\n", len);
	for(size_t i = 0; i < len; i += 4) {
		Debug::Log("esp+%04X = %08X\n", i, R->get_StackVar32(i));
	}
	Debug::Log("Done.\n");
}

//Hook at 0x4068E0 AND 4A4AC0
DEFINE_HOOK(4068E0, Debug_Log, 1)
DEFINE_HOOK_AGAIN(4A4AC0, Debug_Log, 1)
{
	if(Debug::bLog && Debug::pLogFile)
	{
		va_list ArgList = (va_list)(R->get_ESP() + 0x8);
		char* Format = (char*)R->get_StackVar32(0x4);

		vfprintf(Debug::pLogFile, Format, ArgList);
		fflush(Debug::pLogFile);
	}
	return 0x4A4AF9; // changed to co-op with YDE
}
