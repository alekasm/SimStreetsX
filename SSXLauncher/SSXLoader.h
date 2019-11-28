#pragma once
#include <Windows.h>
#include "Shlwapi.h"
#include "shellapi.h"
#include <fstream>
#include <vector>
#include <wincrypt.h>
#include <Windows.h>
#include <algorithm>
#include <regex>
#include "Patcher.h"
#include "GameData.h"
#include "FileVersion.h"
#include "Message.h"

#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "Kernel32.lib")
#pragma comment(lib, "Advapi32.lib" )

struct SSXParameters
{
	bool verify_install;
	unsigned int sleep_time;
	unsigned int resolution_mode;
	bool fullscreen;
	bool show_fps;
};

class SSXLoader
{
public:
	static bool LoadFiles();	
	static int GetPatchedSSXVersion();
	static std::string GetStreetsGameLocation();
	static bool CreatePatchedGame(std::string, SSXParameters);
	static bool StartSSX(SSXParameters);	
	static bool GetValidInstallation();
	static const unsigned int SSX_VERSION = 2;
private:
	static bool GetFileCompatability(std::string);	
};