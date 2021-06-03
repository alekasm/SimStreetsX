#pragma once
#include <Windows.h>
#include <vector>
#include <string>
#include "Patcher.h"
#include "GameVersion.h"

class GameData
{
public:
	static std::vector<Instructions> GenerateData(PEINFO, GameVersions);
private:
	static void CreateSleepFunction(DetourMaster*, GameVersions);
	static void CreateCDFunction(DetourMaster*, GameVersions);
	static void CreateRendererFunction(DetourMaster*, GameVersions);
	static void CreateRenderStaticDashFunction(DetourMaster*, GameVersions);
	static void CreateRenderDynamicDashFunction(DetourMaster*, GameVersions);
	static void CreateGlobalInitFunction(DetourMaster*, GameVersions);
	static void CreateMenuInitFunction(DetourMaster*, GameVersions);
	static void CreateSkyboxArgumentFunction(DetourMaster*, GameVersions);
	static void CreateInitSkyboxFunction(DetourMaster*, GameVersions);
	static void CreateMenusFunction(DetourMaster*, GameVersions);
	static void CreateNoDashFunction(DetourMaster*, GameVersions);
	static void CreateResolutionFunction(DetourMaster*, GameVersions);
	static void CreateTimedFunction(DetourMaster*, GameVersions);
	static void CreateAddFontFunction(DetourMaster*, GameVersions);
};


