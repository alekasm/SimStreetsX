#pragma once
#include <Windows.h>
#include <map>
#include <vector>
#include <string>
#include "Patcher.h"




class GameData
{
public:
	enum FunctionType { MAIN_LOOP, CD_CHECK, DS_SLEEP, VERSIONS, RENDERER };

	enum DWORDType { RENDERER_TYPE, MY_SLEEP };
	enum Version { VCLASSICS };
	static void initialize(PEINFO info);
	static bool PatchGame(std::string game_exe, GameData::Version version);
	static DWORD GetDWORDOffset(GameData::Version version, GameData::DWORDType dword_type);
	static DWORD GetDWORDAddress(Version version, DWORDType dtype);
	
private:
	static DWORD GetFunctionAddress(Version, FunctionType);	
	static void CreateSleepFunction(GameData::Version version);
	static void CreateCDFunction(GameData::Version version);
	static void CreateRendererFunction(GameData::Version version);
};


