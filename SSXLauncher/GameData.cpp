#include "GameData.h"

namespace
{
	struct Game
	{
		std::map<GameData::FunctionType, DWORD> functions;
		std::map<GameData::DWORDType, DWORD> global_dwords;
	};
	std::map<GameData::Version, Game> games;
	DetourMaster* master;
}

bool GameData::PatchGame(std::string game_exe, GameData::Version version)
{
	CreateSleepFunction(version);
	CreateCDFunction(version);
	CreateRendererFunction(version);
	CreateRenderStaticDashFunction(version);
	CreateGlobalInitFunction(version);
	CreateRenderDynamicDashFunction(version);
	CreateMenuInitFunction(version);
	CreateInitSkyboxFunction(version);
	CreateSkyboxArgumentFunction(version);
	CreateMenusFunction(version);
	return Patcher::Patch(master->instructions, game_exe);
}

DWORD GameData::GetDWORDOffset(GameData::Version version, GameData::DWORDType dword_type)
{
	return master->GetFileOffset(games[version].global_dwords[dword_type]);
}

void GameData::CreateCDFunction(GameData::Version version)
{
	DWORD function_entry = GameData::GetFunctionAddress(version, GameData::CD_CHECK);
	Instructions instructions(DWORD(function_entry + 0x19B));
	instructions.jmp(DWORD(function_entry + 0x2B3));    //jmp <location> (originally jnz)

	size_t is_size = instructions.GetInstructions().size();
	printf("[CD Check Bypass] Generated a total of %d bytes\n", is_size);
	master->instructions.push_back(instructions);
}

void GameData::CreateRendererFunction(GameData::Version version)
{
	DWORD function_entry = GameData::GetFunctionAddress(version, GameData::RENDERER);

	/*
	Function 4707C0 is a programming error from the original developers:

	1. The 4707C0 function calls _grSstQueryBoards, a Glide function without first checking the renderer mode.
	A few lines down in function 470480, another function is called (53A580) which properly checks the renderer mode
	first. It appears that 4707C0 is both repetitive and useless when 53A580 exists

	2. For some reason global dword 5E82F8 is only set in the 4707C0 function, and only read after its called. I'm
	not sure why they didn't just use a return (eax)?

	Function 4707C0 is only called in the 'Renderer' (470480) function, and right before 53A580 is called - which does the
	exact same (but safer) checks. Glide does get properly loaded in memory, but will crash because it uses 'out' x86
	instructions which I believe is direct real mode interfacing. Not sure how this even worked on Win95, will need
	to research more.	
	*/

	Instructions instructions(DWORD(function_entry + 0x138));
	instructions << ByteArray{ 0x90, 0x90, 0x90, 0x90, 0x90 };	//nops out: call 0x4707C0
	instructions << ByteArray{ 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 }; //nops out: cmp dword_5E82F8, 0 
	instructions << ByteArray{ 0xEB, 0x3F }; //replaced jz with jmp, bypasses 53A580 as its only needed for glide

	size_t is_size = instructions.GetInstructions().size();
	printf("[Software Renderer Patch] Generated a total of %d bytes\n", is_size);
	master->instructions.push_back(instructions);

	DWORD r_address = GameData::GetDWORDAddress(version, GameData::RENDERER_TYPE);
	master->instructions.push_back(DataValue(r_address, 0x00));
}

void GameData::CreateSleepFunction(GameData::Version version)
{
	DWORD function_entry = GameData::GetFunctionAddress(version, GameData::MAIN_LOOP);
	DWORD dsfunction_sleep = GameData::GetFunctionAddress(version, GameData::DS_SLEEP);
	DWORD sleep_param = master->base_location + 0x4;

	Instructions instructions(DWORD(function_entry + 0x128));
	instructions.jmp(master->GetNextDetour());                      //jmp <detour> 
	instructions << ByteArray{ 0xFF, 0xD7 };						//call edi
	instructions << BYTE(0x50);										//push eax
	instructions.push_rm32(sleep_param);                            //push param millis
	instructions.call_rm32(dsfunction_sleep);						//call Sleep
	instructions << BYTE(0x58);										//pop eax
	instructions << ByteArray{ 0x85, 0xC0 };						//test eax, eax
	instructions.jnz(DWORD(function_entry + 0x12E));				//jnz <function>
	instructions.jmp(DWORD(function_entry + 0x152));				//jmp <function>

	size_t is_size = instructions.GetInstructions().size();
	master->SetLastDetourSize(is_size);
	printf("[Main Loop Sleep] Generated a total of %d bytes\n", is_size);
	master->instructions.push_back(instructions);
}

void GameData::CreateRenderStaticDashFunction(GameData::Version version)
{
	DWORD function_entry = GameData::GetFunctionAddress(version, GameData::RENDER_STATIC_DASH);
	DWORD renderw_address = GameData::GetDWORDAddress(version, GameData::RENDER_AREA_WIDTH);
	DWORD renderh_address = GameData::GetDWORDAddress(version, GameData::RENDER_AREA_HEIGHT);
	DWORD light2_address = GameData::GetDWORDAddress(version, GameData::LIGHT_2_PTR);

	Instructions instructions(DWORD(function_entry + 0x5A7)); //0x59B = logic block
	//mov eax, [light1_address]
	//mov ecx, [eax+C]
	//mov edx, [eax+8]
	//push ecx
	instructions << BYTE(0x52);	//push edx
	instructions << ByteArray{ 0x8B, 0x15 }; 
	instructions << renderh_address;	//mov edx, [render height address]
	instructions << ByteArray{ 0x2B, 0xD1 }; // sub edx, ecx
	instructions << ByteArray{ 0x6A, 0x00 }; //push 0
	instructions << ByteArray{ 0x6A, 0x00 }; //push 0
	instructions << BYTE(0x52);	//push edx (y)
	instructions << ByteArray{ 0x6A, 0x00 }; // push 0 (x)
	instructions << BYTE(0x55);	//push ebp
	instructions << ByteArray{ 0x8B, 0xC8 }; //mov ecx, eax
	instructions << ByteArray{ 0x8B, 0x18 }; //mov ebx, [eax]
	instructions << ByteArray{ 0xFF, 0x53, 0x3C }; //call [ebx+3C]

	instructions << BYTE(0xA1);
	instructions << light2_address; //mov eax, [light2_address]
	instructions << ByteArray{ 0x8B, 0x48, 0x0C }; //mov ecx, [eax+C]
	instructions << ByteArray{ 0x8B, 0x50, 0x08 }; //mov edx, [eax+8]
	instructions << BYTE(0x51); //push ecx
	instructions << BYTE(0x52); //push edx
	instructions << ByteArray{ 0x8B, 0x0D };
	instructions << renderw_address; //mov ecx, [render width address]
	instructions << ByteArray{ 0x8B, 0x15 };
	instructions << renderh_address; //mov edx, [render height address]
	instructions << ByteArray{ 0x2B, 0x50, 0x0C }; //sub edx, [eax+C]
	instructions << ByteArray{ 0x2B, 0x48, 0x08 }; //sub ecx, [eax+8]
	instructions << ByteArray{ 0x6A, 0x00 }; //push 0
	instructions << ByteArray{ 0x6A, 0x00 }; //push 0
	instructions << BYTE(0x52);	//push edx (y)
	instructions << BYTE(0x51);	//push ecx (y)
	instructions << BYTE(0x55);	//push ebp
	instructions << ByteArray{ 0x8B, 0xC8 }; //mov ecx, eax
	instructions << ByteArray{ 0x8B, 0x18 }; //mov ebx, [eax]
	instructions << ByteArray{ 0xFF, 0x53, 0x3C }; //call [ebx+3C]
	instructions << ByteArray{ 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 }; //nop padding

	size_t is_size = instructions.GetInstructions().size();
	master->SetLastDetourSize(is_size);
	printf("[Render Static Dash] Generated a total of %d bytes\n", is_size);
	master->instructions.push_back(instructions);
}

void GameData::CreateRenderDynamicDashFunction(GameData::Version version)
{
	DWORD function_entry = GameData::GetFunctionAddress(version, GameData::RENDER_DYNAMIC_DASH);
	DWORD renderw_address = GameData::GetDWORDAddress(version, GameData::RENDER_AREA_WIDTH);
	DWORD renderh_address = GameData::GetDWORDAddress(version, GameData::RENDER_AREA_HEIGHT);

	/*
	esi+6E = radar.x
	esi+70 = radar.y
	dword_6EF340 = timer.x
	dword_6EF33C = timer.y
	esi+4053E = health.x
	esi+40540 = health.y
	esi+38 = weapons.x
	esi+3A = weapons.y
	esi+3C = weapons slot amount
	esi+40362 = fuel line length
	esi+4036E = fuel.x
	esi+40372 = fuel.y
	esi+40396 = power line length
	esi+403A2 = power.x
	esi+403A6 = power.y
	dword_5E79B0 = speedometer.x
	dword_5E79B4 = speedometer.y

	Unknowns:
	esi+0C (bool)
	esi+4036A
	esi+40392 (power rotation?)
	esi+4035E (fuel rotation?)
	esi+4039E
	*/

	//TODO Replace static addresses for timer x/y, speedometer
	//health, radar, and weapons use WORD, the rest use DWORD

	Instructions instructions(DWORD(function_entry + 0x46));
	instructions.jmp(master->GetNextDetour()); //jmp <detour> 

	//Fill in all the x values first
	instructions << BYTE(0xA1);
	instructions << renderw_address; //mov eax, [render width]
	instructions << ByteArray{ 0x83, 0xE8, 0x7A }; //sub eax, 0x7A
	instructions << ByteArray{ 0x66, 0x89, 0x46, 0x6E }; //mov word ptr [esi+6E], ax
	instructions << BYTE(0xA3);
	instructions << DWORD(0x6EF340); //mov dword ptr [0x6EF340], eax
	instructions << ByteArray{ 0x83, 0xE8, 0x61 }; //sub eax, 0x61
	instructions << ByteArray{ 0x66, 0x89, 0x46, 0x38 }; //mov word ptr [esi+38], ax
	instructions << ByteArray{ 0x83, 0xE8, 0x2D }; //sub eax, 0x2D
	instructions << ByteArray{ 0x89, 0x86, 0xA2, 0x03, 0x04, 0x00 }; //mov dword ptr [esi+403A2], eax
	instructions << ByteArray{ 0x83, 0xE8, 0x38 }; //sub eax, 0x38
	instructions << ByteArray{ 0x89, 0x86, 0x6E, 0x03, 0x04, 0x00 }; //mov dword ptr [esi+4036E], eax
	instructions << ByteArray{ 0x66, 0xC7, 0x86, 0x3E, 0x05, 0x04, 0x00, 0x0F, 0x00 }; //mov word ptr [esi+4053E], 0xF

	//Fill in all the y values second
	instructions << BYTE(0xA1);
	instructions << renderh_address; //mov eax, [render height]
	instructions << ByteArray{ 0x83, 0xE8, 0x1F }; //sub eax, 0x61
	instructions << ByteArray{ 0x89, 0x86, 0x72, 0x03, 0x04, 0x00 }; //mov dword ptr [esi+40372], eax
	instructions << ByteArray{ 0x89, 0x86, 0xA6, 0x03, 0x04, 0x00 }; //mov dword ptr [esi+403A6], eax
	instructions << ByteArray{ 0x83, 0xE8, 0x0F }; //sub eax, 0xF
	instructions << BYTE(0xA3);
	instructions << DWORD(0x5E79B4); //mov dword ptr [0x5E79B4], eax (TODO REPLACE STATIC ADDR)
	instructions << ByteArray{ 0x83, 0xE8, 0x38 }; //sub eax, 0x38
	instructions << ByteArray{ 0x66, 0x89, 0x46, 0x70 }; //mov word ptr [esi+70], ax
	instructions << ByteArray{ 0x83, 0xE8, 0x02 }; //sub eax, 0x2
	instructions << ByteArray{ 0x66, 0x89, 0x46, 0x3A }; //mov word ptr [esi+70], ax
	instructions << ByteArray{ 0x83, 0xE8, 0x13 }; //sub eax, 0x13
	instructions << ByteArray{ 0x66, 0x89, 0x86, 0x40, 0x05, 0x04, 0x00 }; //mov word ptr [esi+40540], ax
	instructions << ByteArray{ 0x83, 0xE8, 0x06 }; //sub eax, 0x06
	instructions << BYTE(0xA3);
	instructions << DWORD(0x6EF33C); //mov dword ptr [0x6EF33C], eax

	//set fuel length, power length, and weapon slot amount
	instructions << ByteArray{ 0x66, 0xC7, 0x46, 0x3C, 0x03, 0x00 }; //mov word ptr [esi+3C], 3
	instructions << ByteArray{ 0xC7, 0x86, 0x62, 0x03, 0x04, 0x00, 0x13, 0x00, 0x00, 0x00 }; //mov [esi+40362], 0x13
	instructions << ByteArray{ 0xC7, 0x86, 0x96, 0x03, 0x04, 0x00, 0x13, 0x00, 0x00, 0x00 }; //mov [esi+40362], 0x13

	//set the unknowns
	instructions << ByteArray{ 0xC7, 0x86, 0x5E, 0x03, 0x04, 0x00, 0x96, 0x00, 0x00, 0x00 }; //mov [esi+4035E], 0x96
	instructions << ByteArray{ 0xC7, 0x86, 0x92, 0x03, 0x04, 0x00, 0x96, 0x00, 0x00, 0x00 }; //mov [esi+40392], 0x96
	instructions << ByteArray{ 0xC7, 0x86, 0x6A, 0x03, 0x04, 0x00, 0x9A, 0x99, 0x99, 0x3F }; //mov [esi+4036A], 0x3F99999A
	instructions << ByteArray{ 0xC7, 0x86, 0x9E, 0x03, 0x04, 0x00, 0x9A, 0x99, 0x99, 0x3F }; //mov [esi+4039E], 0x3F99999A

	instructions << BYTE(0x5E); //pop esi
	instructions << ByteArray{ 0xC2, 0x04, 0x00 }; //ret 4

	size_t is_size = instructions.GetInstructions().size();
	master->SetLastDetourSize(is_size);
	printf("[Render Dynamic Dash] Generated a total of %d bytes\n", is_size);
	master->instructions.push_back(instructions);
}

void GameData::CreateMenuInitFunction(GameData::Version version)
{
	/*
		Originally this function checked the resolution type and populated a master menu object with
		fields corresponding to the same values in Resolution Lookup (0x5E7954). I'm not sure why
		they didn't just call the resolution lookup function instead of doing the identical check again.
		In this case we do NOT want to do a resolution lookup, we want the resolution of the menus to always
		be 640x480. It appears this part of the code was written with multiple resolutions in mind, however
		it wasn't tested/working fully. 
	*/
	DWORD function_entry = GameData::GetFunctionAddress(version, GameData::MENU_INIT);

	Instructions instructions(DWORD(function_entry + 0x176));
	instructions << ByteArray{ 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 }; //nops out cmp dword_5e7898, 1
	instructions << ByteArray{ 0x90, 0x90 }; //nops out jnz short loc_42e9F7

	size_t is_size = instructions.GetInstructions().size();
	master->SetLastDetourSize(is_size);
	printf("[Menu Initialization] Generated a total of %d bytes\n", is_size);
	master->instructions.push_back(instructions);
}


void GameData::CreateGlobalInitFunction(GameData::Version version)
{
	DWORD function_entry = GameData::GetFunctionAddress(version, GameData::GLOBAL_INIT); 
	DWORD function_res = GameData::GetFunctionAddress(version, GameData::RES_LOOKUP);
	//Just using these addresses as to load the width/height into, these will eventually
	//get overwritten by themselves (as a result of this patch).
	DWORD renderw_address = GameData::GetDWORDAddress(version, GameData::RENDER_AREA_WIDTH);
	DWORD renderh_address = GameData::GetDWORDAddress(version, GameData::RENDER_AREA_HEIGHT);
	DWORD skyboxh_address = GameData::GetDWORDAddress(version, GameData::SKYBOX_HEIGHT);


	Instructions instructions(DWORD(function_entry + 0x90));
	instructions.jmp(master->GetNextDetour()); //jmp <detour> 
	instructions << ByteArray{ 0x8D, 0x05 };
	instructions << renderw_address; //lea eax, [render width]
	instructions << ByteArray{ 0x8D, 0x0D };
	instructions << renderh_address; //lea ecx, [render height]	
	instructions << BYTE(0x51); //push ecx 
	instructions << BYTE(0x50); //push eax 	
	instructions.call(function_res);
	instructions << ByteArray{ 0x8B, 0x09 }; //mov ecx, [ecx]
	instructions << ByteArray{ 0x8B, 0x00 }; //mov eax, [eax] 
	instructions << ByteArray{ 0xBF, 0x08, 0x00, 0x00, 0x00 }; //mov edi, 8

	instructions << ByteArray{ 0x89, 0x86, 0x86, 0x4B, 0x00, 0x00 }; //mov [esi+4B86], eax 
	instructions << ByteArray{ 0x89, 0x8E, 0x8A, 0x4B, 0x00, 0x00 }; //mov [esi+4B8A], ecx
	instructions << ByteArray{ 0x89, 0xBE, 0x8E, 0x4B, 0x00, 0x00 }; //mov [esi+4B8E], edi 

	instructions << ByteArray{ 0x89, 0x86, 0x92, 0x4B, 0x00, 0x00 }; //mov [esi+4B92], eax 	//physical screen width
	instructions << ByteArray{ 0x89, 0x8E, 0x96, 0x4B, 0x00, 0x00 }; //mov [esi+4B96], ecx  //physical screen height
	instructions << ByteArray{ 0x89, 0xBE, 0x9A, 0x4B, 0x00, 0x00 }; //mov [esi+4B9A], edi  //color depth

	//Menu Fixes
	//Set render width into car factory and main car loadout, however use 640 for the car lot view
	instructions << BYTE(0xA3);
	instructions << GameData::GetDWORDAddress(version, GameData::MENU_PTR_CAR_FACTORY_WIDTH);

	instructions << BYTE(0xA3);
	instructions << GameData::GetDWORDAddress(version, GameData::MENU_PTR_MAIN_LOADOUT_WIDTH);

	instructions << ByteArray{ 0xC7, 0x05 };
	instructions << GameData::GetDWORDAddress(version, GameData::MENU_PTR_UNKNOWN) + 0x14; //view width
	instructions << DWORD(0x280); 

	instructions << ByteArray{ 0xC7, 0x05 };
	instructions << GameData::GetDWORDAddress(version, GameData::MENU_PTR_UNKNOWN) + 0x8; //size width
	instructions << DWORD(0x280);

	instructions << ByteArray{ 0xC7, 0x05 };
	instructions << GameData::GetDWORDAddress(version, GameData::MENU_PTR_UNKNOWN) + 0xC; //size height
	instructions << DWORD(0x1E0);	

	instructions << ByteArray{ 0xC7, 0x86, 0x6A, 0x4B, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00 }; //mov [esi + 4B6A], 1
	instructions << ByteArray{ 0xC7, 0x86, 0xB6, 0x4B, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00 }; //mov [esi + 4BB6], 1
	instructions << ByteArray{ 0xC7, 0x86, 0xF6, 0x4B, 0x00, 0x00, 0x00, 0x00, 0x20, 0x41 }; //mov [esi + 4BF6], 41200000

	//Skybox height calculation
	instructions << BYTE(0x50); //push eax
	instructions << ByteArray{ 0x8B, 0xC8 }; //mov ecx, eax
	instructions << ByteArray{ 0x33, 0xD2 }; //xor edx, edx
	instructions << ByteArray{ 0xB8, 0x00, 0xF4, 0x01, 0x00 }; //mov eax, 1F400 (128K)
	instructions << ByteArray{ 0xF7, 0xF1 }; //div ecx
	instructions << BYTE(0xA3);
	instructions << skyboxh_address; //mov skybox_address, eax
	instructions << BYTE(0x58); //pop eax

	instructions << BYTE(0x55); //push ebp
	instructions << ByteArray{ 0x8D, 0x86, 0x9E, 0x4B, 0x00, 0x00 }; //lea eax, [esi+4B9E]
	instructions << BYTE(0x50); //push eax
	instructions.call(DWORD(0x54F6B0)); //call function

	instructions << ByteArray{ 0x8D, 0xBE, 0x86, 0x4B, 0x00, 0x00 }; //lea edi, [esi+4B86]
	instructions << ByteArray{ 0x8D, 0x9E, 0x92, 0x4B, 0x00, 0x00 }; //lea ebx, [esi+4B92]	

	instructions.jmp(function_entry + 0xF9); //jmp back

	size_t is_size = instructions.GetInstructions().size();
	master->SetLastDetourSize(is_size);
	printf("[Global Initialization] Generated a total of %d bytes\n", is_size);
	//printf("DetourMaster now points to address starting at %x\n", master->current_location);
	master->instructions.push_back(instructions);
}

void GameData::CreateInitSkyboxFunction(GameData::Version version)
{
	/*
	Unfortunately this function was coded poorly. It takes in 3 arguments:
	sub_54C780(dword_5E7820, skybox_width, skybox_height) and is only called in one location (0x4a1BC4).
	Even though it takes in the skybox width/height as parameters, it still manually assigns a hardcoded
	skybox width and height. 

	This patch corrects this logic by properly using the function arguments...

	*/

	DWORD function_entry = GameData::GetFunctionAddress(version, GameData::INITIALIZE_SKYBOX);

	Instructions instructions(DWORD(function_entry + 0x42));
	DWORD next_detour = master->GetNextDetour();
	instructions.jmp(next_detour, FALSE); //jmp <detour> 
	instructions.nop(15); //clears the next 15 bytes to clean up debugging
	instructions.relocate(next_detour);
	instructions << ByteArray{ 0x8B, 0x45, 0x0C }; //mov eax, [ebp+0xC] (sky width arg)
	instructions << ByteArray{ 0x89, 0x86, 0x3C, 0x01, 0x00, 0x00 }; //mov [esi+0x13C], eax
	instructions << ByteArray{ 0x8B, 0x45, 0x10 }; //mov eax, [ebp+0x10] (sky height arg)
	instructions << ByteArray{ 0x89, 0x86, 0x40, 0x01, 0x00, 0x00 }; //mov [esi+0x140], eax
	instructions.jmp(function_entry + 0x56); //jmp back

	size_t is_size = instructions.GetInstructions().size();
	master->SetLastDetourSize(is_size);
	printf("[Skybox Initialization] Generated a total of %d bytes\n", is_size);
	master->instructions.push_back(instructions);
}

void GameData::CreateMenusFunction(GameData::Version version)
{
	/*
	The "Car Lot" view requires a render width of 640 (for some reason). The menu struct pointer
	which is passed into the menu processor (0x4A9BA5) is the same for the car lot and main car
	menu. This change uses a different menu struct pointer, however I believe we can make an entirely
	new menu struct if needed (the one I'm using is not really used however).
	*/


	//MENU_PTR_UNKNOWN_WIDTH populated in global init
	DWORD function_entry = GameData::GetFunctionAddress(version, GameData::MENU_CAR_LOT);
	Instructions instructions(DWORD(function_entry + 0x587)); 
	instructions << BYTE(0x68);
	instructions << GameData::GetDWORDAddress(version, GameData::MENU_PTR_UNKNOWN);

	printf("[Menus Patch] Generated a total of %d bytes\n", instructions.GetInstructions().size());
	master->instructions.push_back(instructions);
}

void GameData::CreateSkyboxArgumentFunction(GameData::Version version)
{
	/*
	Since the arguments for the skybox width/height are hardcoded multiple times in various functions,
	a new function (not a detour) is created which will serve as an argument populator.

	Example Previous 1: SomeSkyboxFunctionA(bool a, DWORD_PTR b, int sky_width, int sky_height)
	Example Previous 2: SomeOtherSkyboxFunctionB(int sky_width, int sky_height, int c)

	Example New 1: SomeSkyboxFunctionA(bool a, DWORD_PTR b, MySkyboxFunction())
	Example New 2: SomeOtherSkyboxFunctionB(MySkyboxFunction(), int c)

	The code is the same, however I opted not to create a jump detour like usual because
	the return address would be different - which means that a jump detour requires
	two of the same detour but with different return addresses (hence using a call instead).
	*/
		
	DWORD skybox_height = GameData::GetDWORDAddress(version, GameData::SKYBOX_HEIGHT);
	DWORD render_width = GameData::GetDWORDAddress(version, GameData::RENDER_AREA_WIDTH);

	//This function will occupy both eax and ecx
	DWORD skybox_arg_func = master->GetNextDetour();
	Instructions instructions(skybox_arg_func);
	instructions << BYTE(0x58); //pop eax
	instructions << ByteArray{ 0x8B, 0x0D };
	instructions << skybox_height; //mov ecx, skybox_height
	instructions << BYTE(0x51); //push ecx
	instructions << ByteArray{ 0x8B, 0x0D };
	instructions << render_width; //mov ecx, render_width
	instructions << BYTE(0x51); //push ecx
	instructions << BYTE(0x50); //push eax
	instructions << BYTE(0xC3); //ret

	size_t is_size = instructions.GetInstructions().size();
	master->SetLastDetourSize(is_size);
	printf("[Skybox Arguments] Generated a total of %d bytes\n", is_size);
	master->instructions.push_back(instructions);	

	DWORD render_skybox = GameData::GetFunctionAddress(version, GameData::RENDER_SKYBOX);
	Instructions is_rs(DWORD(render_skybox + 0x17));
	//Changing eax to ebx because we use eax in the skybox arg function
	is_rs << ByteArray{ 0x8B, 0x5C, 0x24, 0x08 }; //mov ebx, [esp+0x8]
	//is_rs << BYTE(0xE8);
	//is_rs << skybox_arg_func; //call skybox arg func
	is_rs.call(skybox_arg_func);
	is_rs.nop(5); //clean up for debuggers
	is_rs.relocate(DWORD(render_skybox + 0x2F));
	is_rs << BYTE(0x53); //push ebx
	master->instructions.push_back(is_rs);
	printf("[Render Skybox] Generated a total of %d bytes\n", is_rs.GetInstructions().size());

	DWORD call_init_skybox = GameData::GetFunctionAddress(version, GameData::CALL_INIT_SKYBOX);
	Instructions is_cis(DWORD(call_init_skybox + 0x42));
	is_cis.nop(5); //Clean up for debuggers
	is_cis.call(skybox_arg_func);
	//is_cis << BYTE(0xE8);
	//is_cis << skybox_arg_func; //call skybox_arg_func
	is_cis << BYTE(0xA1);
	is_cis << DWORD(0x5E7820); // mov eax, [5E7820] (TODO REPLACE STATIC ADDR)
	master->instructions.push_back(is_cis);
	printf("[Call Init Skybox] Generated a total of %d bytes\n", is_cis.GetInstructions().size());
}

DWORD GameData::GetFunctionAddress(Version version, FunctionType ftype)
{
	return games[version].functions[ftype];
}

DWORD GameData::GetDWORDAddress(Version version, DWORDType dtype)
{
	return games[version].global_dwords[dtype];
}

void GameData::initialize(PEINFO info)
{
	if (master)
	{
		delete master;
		master = nullptr;
	}

	master = new DetourMaster(info);
	Patcher::SetDetourMaster(master);

	Game version_classics; 
	version_classics.functions[MAIN_LOOP] = 0x5169B0;
	version_classics.functions[DS_SLEEP] = 0xC53794;
	version_classics.functions[CD_CHECK] = 0x5317F0;
	version_classics.functions[RENDERER] = 0x470480;	
	version_classics.functions[RENDER_STATIC_DASH] = 0x453160;
	version_classics.functions[GLOBAL_INIT] = 0x46F8A0;
	version_classics.functions[RENDER_DYNAMIC_DASH] = 0x450F40;
	version_classics.functions[MENU_INIT] = 0x42E860;
	version_classics.functions[RES_LOOKUP] = 0x49C4C0;

	version_classics.functions[INITIALIZE_SKYBOX] = 0x54C780;
	version_classics.functions[RENDER_SKYBOX] = 0x54CB70;
	version_classics.functions[CALL_INIT_SKYBOX] = 0x4A1B70;
	
	version_classics.global_dwords[RENDERER_TYPE] = 0x6906BC; //0 = Software, 1 = Glide, 2 = OpenGL	
	version_classics.global_dwords[SHOW_DEBUG] = 0x5E7954; //1 = on, 0 = off, loc_4541C8

	version_classics.functions[MENU_CAR_LOT] = 0x425C40;
	version_classics.functions[MENU_CAR_FACTORY] = 0x478CC0;
	version_classics.functions[MENU_MAIN_LOADOUT] = 0x413500;

	version_classics.global_dwords[MENU_PTR_CAR_FACTORY] = 0x5E87C0;
	version_classics.global_dwords[MENU_PTR_CAR_FACTORY_WIDTH] = 0x5E87D4; //base + 0x14
	version_classics.global_dwords[MENU_PTR_MAIN_LOADOUT] = 0x5E6A88;
	version_classics.global_dwords[MENU_PTR_MAIN_LOADOUT_WIDTH] = 0x5E6A9C; //base + 0x14
	version_classics.global_dwords[MENU_PTR_UNKNOWN] = 0x697830;
	version_classics.global_dwords[MENU_PTR_UNKNOWN_WIDTH] = 0x697844; //base +0x14	

	version_classics.global_dwords[LIGHT_1_PTR] = 0x5E7970;
	version_classics.global_dwords[LIGHT_2_PTR] = 0x5E7974;
	version_classics.global_dwords[RENDER_AREA_WIDTH] = 0x5E7880;
	version_classics.global_dwords[RENDER_AREA_HEIGHT] = 0x5E7884;
	version_classics.global_dwords[RES_TYPE] = 0x5E7898; //Default = 1, 640x480

	//---------- IN PROGRESS
	version_classics.functions[VERSIONS] = 0x470F90;	
	version_classics.functions[MENU_STRUCT_PROCESSOR] = 0x4A9BA5;

	//---------- Below here contains completely new functions/variables
	version_classics.global_dwords[MY_SLEEP] = master->base_location + 0x4;
	version_classics.global_dwords[SKYBOX_HEIGHT] = master->base_location + 0x8;
	games[VERSION_1_0] = version_classics;

}