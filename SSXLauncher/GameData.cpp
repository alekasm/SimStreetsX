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


void GameData::CreateGlobalInitFunction(GameData::Version version)
{
	DWORD function_entry = GameData::GetFunctionAddress(version, GameData::GLOBAL_INIT); 
	DWORD function_res = GameData::GetFunctionAddress(version, GameData::RES_LOOKUP);
	//Just using these addresses as to load the width/height into, these will eventually
	//get overwritten by themselves (as a result of this patch).
	DWORD renderw_address = GameData::GetDWORDAddress(version, GameData::RENDER_AREA_WIDTH);
	DWORD renderh_address = GameData::GetDWORDAddress(version, GameData::RENDER_AREA_HEIGHT);


	//The detour function:
	//46f92a
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

	instructions << ByteArray{ 0x89, 0x86, 0x92, 0x4B, 0x00, 0x00 }; //mov [esi+4B92], eax 	
	instructions << ByteArray{ 0x89, 0x8E, 0x96, 0x4B, 0x00, 0x00 }; //mov [esi+4B96], ecx
	instructions << ByteArray{ 0x89, 0xBE, 0x9A, 0x4B, 0x00, 0x00 }; //mov [esi+4B9A], edi 

	instructions << ByteArray{ 0xC7, 0x86, 0x6A, 0x4B, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00 }; //mov [esi + 4B6A], 1
	instructions << ByteArray{ 0xC7, 0x86, 0xB6, 0x4B, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00 }; //mov [esi + 4BB6], 1
	instructions << ByteArray{ 0xC7, 0x86, 0xF6, 0x4B, 0x00, 0x00, 0x00, 0x00, 0x20, 0x41 }; //mov [esi + 4BF6], 41200000

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
	
	version_classics.global_dwords[RENDERER_TYPE] = 0x6906BC; //0 = Software, 1 = Glide, 2 = OpenGL	
	version_classics.global_dwords[SHOW_DEBUG] = 0x5E7954; //1 = on, 0 = off, loc_4541C8
	

	version_classics.global_dwords[LIGHT_1_PTR] = 0x5E7970;
	version_classics.global_dwords[LIGHT_2_PTR] = 0x5E7974;
	version_classics.global_dwords[RENDER_AREA_WIDTH] = 0x5E7880;
	version_classics.global_dwords[RENDER_AREA_HEIGHT] = 0x5E7884;


	//---------- IN PROGRESS
	version_classics.functions[VERSIONS] = 0x470F90;
	version_classics.global_dwords[RES_TYPE] = 0x5E7898; //Default = 1, 640x480
	version_classics.functions[RES_LOOKUP] = 0x49C4C0;	

	//---------- Below here contains completely new functions/variables
	version_classics.global_dwords[MY_SLEEP] = master->base_location + 0x4;
	games[VERSION_1_0] = version_classics;

}