#include "SSXLoader.h"

std::string LastErrorString();
std::string CreateMD5Hash(std::string);
std::vector<std::string> split_string(char delim, std::string split_string);
MessageValue VerifyOriginalGame(std::string source);
std::string FormatDirectoryLocation(std::string full_exe_location);
MessageValue VerifyInstallationDirectory(std::string game_location);
MessageValue VerifyInstallation();
MessageValue CopyFileSafe(std::string source, std::string destination);


namespace
{
	const std::string PATCH_FILE = "SSXPatch.dat";
	const std::string GAME_FILE = "Streets.exe";
	const std::string PATCH_NAME = "SimStreetsX";
	
	std::map<std::string, GameVersions> version_hashes =
	{
		{"df473d94fb4d6741628d58c251224c02", GameVersions::VCLASSICS},
	};

	std::map<GameVersions, std::string> version_description = 
	{
		{GameVersions::VCLASSICS, "Version 1.0 - October 21, 1997"},
	};

	std::string SimStreetsXDirectory;
	std::string SimStreetsGameLocation = "";
	std::string SimStreetsGameInstallDirectory = "";
	bool ValidInstallation = false;

	std::string patched_hash;
	int patched_ssxversion = -1;

	PEINFO peinfo;
	GameVersions game_version;
	FileVersion fileVersion;
}

bool SSXLoader::GetFileCompatability(std::string game_location)
{
	HKEY hKey;
	LONG lRes = RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows NT\\CurrentVersion\\AppCompatFlags\\Layers", 0, KEY_READ, &hKey);
	CHAR lpData[512];
	DWORD lpcbData = sizeof(lpData);
	std::string format_location(game_location);
	std::replace(format_location.begin(), format_location.end(), '/', '\\');
	LSTATUS status = RegQueryValueEx(hKey, format_location.c_str(), 0, NULL, (LPBYTE)lpData, &lpcbData);
	if (status != ERROR_SUCCESS)
	{
		OutputDebugString(std::string("Registry Error: " + std::to_string((int)status) + "\n").c_str());
		OutputDebugString(std::string(format_location + "\n").c_str());
		return false;
	}
	return std::string(lpData).find("256COLOR") != std::string::npos;
}

int SSXLoader::GetPatchedSSXVersion()
{
	return patched_ssxversion;
}

bool SSXLoader::GetValidInstallation()
{
	return ValidInstallation;
}

std::string SSXLoader::GetStreetsGameLocation()
{
	return SimStreetsGameLocation;
}

std::string SCXDirectory(std::string sappend)
{
	return std::string(SimStreetsXDirectory).append(sappend);
}

MessageValue VerifyOriginalGame(std::string source)
{
	if (!PathFileExistsA(source.c_str()))
	{
		return MessageValue(FALSE, "The game does not exist at " + source + "\n");
	}

	std::string hash_check = CreateMD5Hash(source);
	auto it = version_hashes.find(hash_check);

	StringFileInfoMap sfiMap;
	MessageValue result = fileVersion.GetSCFileVersionInfo(source.c_str(), sfiMap);
	std::string sfi_result = "";
	if (result.Value)
	{
		sfi_result += "\nFile Information\n";
		for (auto sfit = sfiMap.begin(); sfit != sfiMap.end(); sfit++)
		{
			sfi_result += sfit->first + ": " + sfit->second + "\n";
		}
	}
	else
	{
		sfi_result += "\nUnable to query the Streets.exe File Information because: \n";
		sfi_result += result.Message;
	}	
	
	if(it == version_hashes.end())
	{
		std::string no_match = PATCH_NAME + " does not have a matching hash stored in the database of patched versions.\n";
		no_match += "Hash: " + hash_check + "\n";
		no_match += sfi_result;
		return MessageValue(FALSE, no_match);
	}

	game_version = it->second;
	return MessageValue(TRUE, sfi_result);
}

bool CreatePatchFile()
{
	std::string hash_reference = CreateMD5Hash(SimStreetsGameLocation);
	OutputDebugString(std::string("Created the hash reference: " + hash_reference + "\n").c_str());
	patched_hash = hash_reference;
	std::ofstream patch_stream(SCXDirectory(PATCH_FILE));
	if (patch_stream.is_open())
	{
		patch_stream << hash_reference << "," << patched_ssxversion << ",";
		patch_stream << SimStreetsGameLocation << "," << static_cast<int>(game_version) << ",";
		patch_stream << static_cast<int>(ValidInstallation);
		patch_stream.close();
		return true;
	}
	return false;
}

bool SSXLoader::CreatePatchedGame(std::string game_location, SSXParameters params)
{
	OutputDebugString(std::string("Attempting to patch game at " + game_location + "\n").c_str());

	if (game_location.empty()) //Means nothing was selected
		return false;

	/*
	If the game we selected is not an original, check to see if our directory has an original.
	If the game we selected is an original, copy it to our SimStreetsX directory
	*/
	bool using_backup_copy = false;
	MessageValue verifyCurrentValue;

	std::string GameLocation = FormatDirectoryLocation(game_location);

	if (!(verifyCurrentValue = VerifyOriginalGame(GameLocation)).Value)
	{
		if (!params.verify_install)
		{
			std::string message_body = "The Streets of SimCity game you selected isn't supported or is already modified/patched. Not ";
			message_body += "attempting to use a backup copy since 'Verify Install' was not selected.\n\n";
			message_body += verifyCurrentValue.Message;
			ShowMessage(PATCH_NAME + " Error", message_body);
			return false;
		}

		if (!VerifyOriginalGame(SCXDirectory("Streets.exe")).Value)
		{
			std::string message_body = "The Streets of SimCity game you selected isn't supported or is already modified/patched. SimStreetsX ";
			message_body += "could not find an original backup. If this is an official version of the game, please submit the following ";
			message_body += "file information so it can be supported.\n\n";
			message_body += verifyCurrentValue.Message;
			ShowMessage(PATCH_NAME + " Error", message_body);
			return false;
		}

		using_backup_copy = true;
	}
	else
	{
		MessageValue copy_result = CopyFileSafe(GameLocation, SCXDirectory(GAME_FILE));
		if (!copy_result.Value)
		{
			ShowMessage(LastErrorString(), copy_result.Message);
			return false;
		}
	}

	if (params.verify_install)
	{
		MessageValue msg_verify;
		if (!(msg_verify = VerifyInstallationDirectory(GameLocation)).Value)
		{
			ShowMessage(PATCH_NAME + " Installation", msg_verify.Message);
			return false;
		}

		MessageValue msg_install;
		if (!(msg_install = VerifyInstallation()).Value)
		{
			ShowMessage(PATCH_NAME + " Installation", msg_install.Message);
			return false;
		}

		ValidInstallation = true;
	}

	MessageValue copy_result2 = CopyFileSafe(SCXDirectory(GAME_FILE), SCXDirectory("SimStreetsX.exe"));
	if (!copy_result2.Value)
	{
		ShowMessage(LastErrorString(), "Failed to create the intermediary " + PATCH_NAME + " patch game file.\n\n" + copy_result2.Message);
		return false;
	}

	PEINFO info;
	if (!Patcher::CreateDetourSection(SCXDirectory("SimStreetsX.exe").c_str(), &info))
	{
		ShowMessage(PATCH_NAME + " Patch Failed", "Failed to modify the game's executable file.\n Make sure the game isn't running or opened in another application");
		return false;
	}

	std::vector<Instructions> instructions = GameData::GenerateData(info, game_version);
	DWORD sleep_address = info.GetDetourVirtualAddress(DetourOffsetType::MY_SLEEP);
	DWORD res_address = Versions[game_version]->data.RES_TYPE;
	DWORD debug_address = Versions[game_version]->data.SHOW_DEBUG;
	instructions.push_back(DataValue(sleep_address, BYTE(params.sleep_time)));
	instructions.push_back(DataValue(res_address, BYTE(params.resolution_mode)));	
	instructions.push_back(DataValue(debug_address, params.show_fps));

	if (!Patcher::Patch(info, instructions, SCXDirectory("SimStreetsX.exe")))
	{
		ShowMessage(PATCH_NAME + " Patch Failed", "Failed to patch the game file.\n Make sure the game isn't running or opened in another application");
		return false;
	}

	SimStreetsGameLocation = params.verify_install ? SimStreetsGameInstallDirectory + "Streets.exe" : GameLocation;

	MessageValue copy_result3 = CopyFileSafe(SCXDirectory("SimStreetsX.exe"), SimStreetsGameLocation);
	if (!copy_result3.Value)
	{
		ShowMessage(LastErrorString(), "Failed to copy the intermediary " + PATCH_NAME + " patch game file to the source directory.\n\n" + copy_result3.Message);
		return false;
	}

	patched_ssxversion = SSX_VERSION;

	BOOL delete_result = DeleteFileA(SCXDirectory("SimStreetsX.exe").c_str());
	if (!delete_result)
	{
		OutputDebugString(std::string("Failed to delete intermediary " + PATCH_NAME + " game file. Still successful... Error: (" + std::to_string(GetLastError()) + ")").c_str());
	}

	if (params.verify_install && !CreatePatchFile())
	{
		ShowMessage(PATCH_NAME + " Error", "Failed to create the patch file which stores information about your specific patch.");
		return false;
	}

	std::string message = "";
	if (using_backup_copy)
		message += "Used Backup: " + version_description[game_version] + "\n";
	else
		message += "Detected Version: " + version_description[game_version] + "\n";

	message += "Patch location : \n" + game_location + "\n\n";
	if (params.verify_install)
		message += "If you modify or move this file, you will need to re-patch again.\n";
	else
		message += "You patched this game without 'Verify Install', so you can't launch using SCXLauncher\n";


	ShowMessage(PATCH_NAME + " Patch Successful!", message);
	return true;
}


void ClearPatchFile(std::string reason)
{
	ShowMessage(PATCH_NAME + " Error", reason);
	SimStreetsGameLocation = "";
	patched_hash = -1;
	patched_ssxversion = -1;
	ValidInstallation = false;
	BOOL delete_result = DeleteFileA(SCXDirectory(PATCH_FILE).c_str());
	if (!delete_result)
	{
		OutputDebugString("Failed to delete the patch file \n");
	}
}

MessageValue VerifyInstallationDirectory(std::string game_location)
{
	//This is used for installing the game (if verify install is enabled)
	std::vector<std::string> path = split_string('/', game_location);
	SimStreetsGameInstallDirectory = "";

	for (size_t i = path.size() - 2; i > 0; i--) //i - 1 = game exe
	{
		std::string path_check = "";
		for (size_t j = 0; j < i + 1; j++)
		{
			path_check += path.at(j) + "/";
		}
		//The top-level directory should have autorun.inf
		std::string auto_run = path_check + "autorun.inf";
		OutputDebugString(std::string("Checking: " + auto_run + "\n").c_str());
		if (PathFileExistsA(auto_run.c_str()))
		{
			SimStreetsGameInstallDirectory = path_check;
			OutputDebugString(std::string("Install: " + SimStreetsGameInstallDirectory).append("\n").c_str());
			return MessageValue(TRUE);
		}
	}
	return MessageValue(FALSE, "The game does not appear to be on a valid extracted CD, missing autorun.inf");
}

std::string FormatDirectoryLocation(std::string full_exe_location)
{
	std::string format_location(full_exe_location);
	std::replace(format_location.begin(), format_location.end(), '\\', '/');
	return format_location;	
}

bool VerifyPatchedGame()
{
	if (!PathFileExistsA(SimStreetsGameLocation.c_str()))
	{
		ClearPatchFile(std::string("The game no longer exists where we patched it:\n" + SimStreetsGameLocation + "\nPlease try repatching.").c_str());
		return false;
	}

	std::string hash_check = CreateMD5Hash(SimStreetsGameLocation);
	if (hash_check.compare(patched_hash) != 0)
	{
		ClearPatchFile("The patched game doesn't have a matching hash, this can happen if you modified the patched game or restored it back to the original game. Please try repatching.");
		return false;
	}

	if (patched_ssxversion != SSXLoader::SSX_VERSION)
	{
		ClearPatchFile(std::string("You currently have " + PATCH_NAME + " Version " + std::to_string(SSXLoader::SSX_VERSION) + " however the game was \npreviously patched using Version " +
			std::to_string(patched_ssxversion) + ". Please repatch the game."));
		return false;
	}

	return true;
}

bool SSXLoader::StartSSX(SSXParameters params)
{

	if (patched_ssxversion < 0) //This shouldn't happen because the button should not be enabled
	{
		ShowMessage(PATCH_NAME + " Error", "You need to patch the game before starting.");
		return false;
	}

	if (!ValidInstallation) //This shouldn't happen because the button should not be enabled
	{
		ShowMessage(PATCH_NAME + " Error", "You need to patch the game using 'Verify Install'");
		return false;
	}

	if (!VerifyPatchedGame())
	{
		return false;
	}

	if (!params.fullscreen && !GetFileCompatability(SimStreetsGameLocation))
	{
		const char* message =
			"You must run the Streets.exe in 8-bit color to use Windowed mode!\n\n"
			"1. Right Click Streets.exe -> Properties\n"
			"2. Select the 'Compatibility' tab\n"
			"3. Enable 'Reduced color mode' and select 8-bit/256 color\n"
			"4. Ensure all other options are NOT selected\n"
			"5. Click apply, then try again";
		ShowMessage(PATCH_NAME, std::string(message));
		return false;
	}

	PEINFO info;
	if (!Patcher::CreateDetourSection(SimStreetsGameLocation.c_str(), &info)) //Should grab detour section
	{
		ShowMessage(PATCH_NAME + " Patch Failed", "Failed to modify the game's executable file.\n Make sure the game isn't running or opened in another application");
		return false;
	}

	std::vector<Instructions> instructions;
	DWORD sleep_address = info.GetDetourVirtualAddress(DetourOffsetType::MY_SLEEP);
	DWORD res_address = Versions[game_version]->data.RES_TYPE;
	DWORD debug_address = Versions[game_version]->data.SHOW_DEBUG;
	instructions.push_back(DataValue(sleep_address, BYTE(params.sleep_time)));
	instructions.push_back(DataValue(res_address, BYTE(params.resolution_mode)));
	instructions.push_back(DataValue(debug_address, params.show_fps));

	if (!Patcher::Patch(info, instructions, SimStreetsGameLocation.c_str()))
	{
		ShowMessage(PATCH_NAME + " Patch Failed", "Failed to patch the game file.\n Make sure the game isn't running or opened in another application");
		return false;
	}

	//Can only get the hash at this point as a reference, can't use it to check for complete validty
	//because changing sleep time and resolution mode dwords will change the hash

	CreatePatchFile();

	std::string parameters = params.fullscreen ? "-f" : "-w";
	HINSTANCE hInstance = ShellExecuteA(NULL, "open", SimStreetsGameLocation.c_str(), parameters.c_str(), NULL, SW_SHOWDEFAULT);
	int h_result = reinterpret_cast<int>(hInstance);
	if (h_result <= 31)
	{
		ShowMessage(std::string(PATCH_NAME + " Error (" + std::to_string(h_result) + ")"), std::string("Failed to start the patched game at: \n" + SimStreetsGameLocation));
		return false;
	}
	return true;
}


bool SSXLoader::LoadFiles()
{
	char* buf = nullptr;
	size_t sz = 0;
	if (_dupenv_s(&buf, &sz, "ProgramFiles") > 0 || buf == nullptr)
	{
		return false;
	}

	SimStreetsXDirectory = std::string(buf).append("\\" + PATCH_NAME);
	if (!PathFileExistsA(SimStreetsXDirectory.c_str()))
	{
		if (!CreateDirectoryA(SimStreetsXDirectory.c_str(), NULL))
		{
			//printf("Failed to create directory: %d", GetLastError());
			return false;
		}
	}
	SimStreetsXDirectory.append("\\");
	OutputDebugString(std::string("Directory: ").append(std::string(SimStreetsXDirectory)).append("\n").c_str());

	if (PathFileExistsA(SCXDirectory(PATCH_FILE).c_str()))
	{
		//Reads and verifes that the patchfile is in the correct format
		char szBuff[128];
		bool valid_patchfile = true;
		std::ifstream fin(SCXDirectory(PATCH_FILE));
		if (fin.good())
		{
			fin.getline(szBuff, 128);
			fin.close();
			std::vector<std::string> props = split_string(',', std::string(szBuff));
			if (props.size() == 5)
			{
				patched_hash = props.at(0);
				patched_ssxversion = std::atoi(props.at(1).c_str());
				SimStreetsGameLocation = props.at(2);				
				game_version = static_cast<GameVersions>(std::atoi(props.at(3).c_str()));
				ValidInstallation = std::atoi(props.at(4).c_str());
				OutputDebugString(std::string("Game is patched at: " + SimStreetsGameLocation + "\n").c_str());
				OutputDebugString(std::string("Game version enum: " + std::to_string(static_cast<int>(game_version)) + "\n").c_str());
			}
			else
			{
				valid_patchfile = false;
				ClearPatchFile("The patch file does not appear to be valid, please repatch the game");
			}			
		}

		//Verifies that the actual contents of the file match what we have
		if (valid_patchfile)
		{
			if (!VerifyPatchedGame())
			{
				OutputDebugString("Failed to verify patched game files \n");
			}

		}
	}
	else
	{
		OutputDebugString("There currently does not exist a patch file \n");
	}


	OutputDebugString("Succesfully finished loading files \n");
	return true;
}


std::string CreateMD5Hash(std::string filename_string)
{
	std::wstring filename_wstring = std::wstring(filename_string.begin(), filename_string.end());
	LPCWSTR filename = filename_wstring.c_str();

	DWORD cbHash = 16;
	HCRYPTHASH hHash = 0;
	HCRYPTPROV hProv = 0;
	BYTE rgbHash[16];
	CHAR rgbDigits[] = "0123456789abcdef";
	HANDLE hFile = CreateFileW(filename, GENERIC_READ, FILE_SHARE_READ, NULL,
		OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);

	CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT);
	CryptCreateHash(hProv, CALG_MD5, 0, 0, &hHash);

	BOOL bResult = FALSE;
	DWORD BUFSIZE = 4096;
	BYTE rgbFile[4096];
	DWORD cbRead = 0;
	while (bResult = ReadFile(hFile, rgbFile, BUFSIZE, &cbRead, NULL))
	{
		if (0 == cbRead)
			break;

		CryptHashData(hHash, rgbFile, cbRead, 0);
	}

	std::string md5_hash = "";
	if (CryptGetHashParam(hHash, HP_HASHVAL, rgbHash, &cbHash, 0))
	{
		for (DWORD i = 0; i < cbHash; i++)
		{
			char buffer[3]; //buffer needs terminating null
			sprintf_s(buffer, 3, "%c%c", rgbDigits[rgbHash[i] >> 4], rgbDigits[rgbHash[i] & 0xf]);
			md5_hash.append(buffer);
		}
		CryptDestroyHash(hHash);
		CryptReleaseContext(hProv, 0);
		CloseHandle(hFile);
		return std::string(md5_hash);
	}
	else
	{
		OutputDebugString(std::string("CryptGetHashParam Error: " + std::to_string(GetLastError()) + "\n").c_str());
		return std::string("");
	}
}

MessageValue VerifyInstallation()
{	
	std::string shared_dir = SimStreetsGameInstallDirectory + "shared/";
	std::string exe_dir = SimStreetsGameInstallDirectory + "exe/";
	std::string dest_dir = SimStreetsGameInstallDirectory;

	if (!PathFileExistsA(shared_dir.c_str()))
	{
		return MessageValue(FALSE, "Directory does not exist:\n" + shared_dir);
	}

	std::vector<std::pair<std::string, std::string>> dll_pairs =
	{
		std::pair<std::string, std::string>(shared_dir, "glide2x.dll"),
		std::pair<std::string, std::string>(shared_dir, "GLU32.dll"),
		std::pair<std::string, std::string>(shared_dir, "OPENGL32.dll"),
		std::pair<std::string, std::string>(shared_dir, "fxmemmap.vxd"),
		std::pair<std::string, std::string>(exe_dir, "SMACKW32.DLL")
	};

	for (std::pair<std::string, std::string> dll : dll_pairs)
	{
		std::string dll_dir = dest_dir + dll.second;
		OutputDebugString(std::string("Checking for " + dll_dir + "\n").c_str());
		if (PathFileExistsA(dll_dir.c_str())) 
		{
			OutputDebugString(std::string(dll.second + " already exists in " + dll_dir + "\n").c_str());
			continue;
		}
		std::string dll_copy = dll.first + dll.second;
		if (!PathFileExistsA(dll_copy.c_str()))
		{
			OutputDebugString(std::string("Couldn't find the required dll at: " + dll_copy + "\n").c_str());
			continue;
		}
		MessageValue copy_result = CopyFileSafe(dll_copy, dll_dir);
		if (!copy_result.Value)
		{
			OutputDebugString(copy_result.Message.c_str());
			continue;
		}
		OutputDebugString(std::string("Copied " + dll_copy + " to: " + dll_dir + "\n").c_str());
	}
	
	return MessageValue(TRUE);
}

MessageValue CopyFileSafe(std::string source, std::string destination)
{
	DWORD attributes = GetFileAttributes(source.c_str());
	if (attributes == INVALID_FILE_ATTRIBUTES)
	{
		return MessageValue(FALSE, "Failed to GET file attributes for:\n" + source + "\n");
	}

	if (attributes & (FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN))
	{
		if (SetFileAttributes(source.c_str(), FILE_ATTRIBUTE_NORMAL) == 0)
		{
			return MessageValue(FALSE, "Failed to SET(" + std::to_string(GetLastError()) +
				") file attributes for:\n" + source + "\n");
		}
		OutputDebugString(std::string("Reset file attributes for: " + source + "\n").c_str());
	}

	if (!CopyFileA(source.c_str(), destination.c_str(), FALSE))
	{
		std::string message = "Failed to copy:\n" + source + "\n\n";
		message += "Destination:\n" + destination + "\n\n";
		return MessageValue(FALSE, message);
	}
	OutputDebugString(std::string("Copied " + source + " to: " + destination + "\n").c_str());
	return MessageValue(TRUE);
}

std::vector<std::string> split_string(char delim, std::string split_string)
{
	std::vector<std::string> vector;
	std::string splitter(split_string);
	while (splitter.find(delim) != std::string::npos)
	{
		auto sfind = splitter.find(delim);
		vector.push_back(splitter.substr(0, sfind));
		splitter.erase(0, sfind + 1);
	}
	vector.push_back(splitter);
	return vector;
}
