#pragma once
#include <Windows.h>
#include <map>
#include <vector>
#include <string>
#include "Patcher.h"




class GameData
{
public:
	enum FunctionType { MAIN_LOOP, CD_CHECK, DS_SLEEP,
						VERSIONS, RENDERER, RES_LOOKUP, 
						GLOBAL_INIT, RENDER_STATIC_DASH, RENDER_DYNAMIC_DASH,
						MENU_INIT };

	enum DWORDType { RENDERER_TYPE, MY_SLEEP, RES_TYPE, SHOW_DEBUG, 
					 LIGHT_1_PTR, LIGHT_2_PTR, RENDER_AREA_WIDTH, RENDER_AREA_HEIGHT };

	enum Version { VERSION_1_0 };
	static void initialize(PEINFO info);
	static bool PatchGame(std::string game_exe, GameData::Version version);
	static DWORD GetDWORDOffset(GameData::Version version, GameData::DWORDType dword_type);
	static DWORD GetDWORDAddress(Version version, DWORDType dtype);
	
private:
	static DWORD GetFunctionAddress(Version, FunctionType);	
	static void CreateSleepFunction(GameData::Version version);
	static void CreateCDFunction(GameData::Version version);
	static void CreateRendererFunction(GameData::Version version);
	static void CreateRenderStaticDashFunction(GameData::Version version);
	static void CreateRenderDynamicDashFunction(GameData::Version version);
	static void CreateGlobalInitFunction(GameData::Version version);
	static void CreateMenuInitFunction(GameData::Version version);
};


