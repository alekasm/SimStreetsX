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
						MENU_INIT, INITIALIZE_SKYBOX, RENDER_SKYBOX, CALL_INIT_SKYBOX,
						MENU_STRUCT_PROCESSOR, MENU_CAR_LOT, MENU_MAIN_LOADOUT, MENU_CAR_FACTORY,
						UNKNOWN_INIT_FUNCTION, RENDER_DASH
						};

	enum DWORDType { RENDERER_TYPE, MY_SLEEP, SKYBOX_HEIGHT, RES_TYPE, SHOW_DEBUG, 
					 LIGHT_1_PTR, LIGHT_2_PTR, RENDER_AREA_WIDTH, RENDER_AREA_HEIGHT,
					 MENU_PTR_CAR_FACTORY, 
					 MENU_PTR_MAIN_LOADOUT, 
					 MENU_PTR_UNKNOWN,  SHOULD_RENDER_DASH, MAIN_DASH_PTR
					};

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
	static void CreateSkyboxArgumentFunction(GameData::Version version);
	static void CreateInitSkyboxFunction(GameData::Version version);
	static void CreateMenusFunction(GameData::Version version);
	static void CreateNoDashFunction(GameData::Version version);
};


