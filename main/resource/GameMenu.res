"GameMenu"
{
	"1"
	{
		"label" "Apply classic NS preset"
		"command" "engine nspreset 1"
	}
	"2"
	{
		"label" "Apply NS 3.3 preset"
		"command" "engine nspreset 2"
	}
	"3"
	{
		"label" "Apply competitive preset"
		"command" "engine nspreset 3"
	}
	"4"
	{
		"label" ""
		"command" ""
	}
	"5"
	{
		"label" ""
		"command" ""
	}
	"6"
	{
		"label" "#GameUI_GameMenu_ResumeGame"
		"command" "ResumeGame"
		"OnlyInGame" "1"
	}
	"7"
	{
		"label" "#Menu_ReadyRoom"
		"command" "engine menureadyroom"
		"OnlyInGame" "1"
	}
	"8"
	{
		"label" "#GameUI_GameMenu_Disconnect"
		"command" "Disconnect"
		"OnlyInGame" "1"
	}
//	"4"
//	{
//		"label" "#GameUI_GameMenu_NewGame"
//		"command" "OpenNewGameDialog"
//		"notmulti" "1"
//	}
//	"5"
//	{
//		"label" "#GameUI_GameMenu_LoadGame"
//		"command" "OpenLoadGameDialog"
//		"notmulti" "1"
//	}
//	"6"
//	{
//	"label" "#GameUI_GameMenu_SaveGame"
//	"command" "OpenSaveGameDialog"
//	"notmulti" "1"
//	"OnlyInGame" "1"
//	}
	"9"
	{
		"label" ""
		"command" ""
		"notmulti" "1"
	}
	"10"
	{
 		"label" "#GameUI_GameMenu_PlayerList"
 		"command" "OpenPlayerListDialog"
 		"OnlyInGame" "1"
 		"notmulti" "0"
	}
	"11"
	{
		"label" "#GameUI_GameMenu_CreateServer"
		"command" "OpenCreateMultiplayerGameDialog"
	}
	"12"
	{
		"label" "#GameUI_GameMenu_FindServers"
		"command" "OpenServerBrowser"
	}
	//"11"
	//{
		//"name" "LoadDemo"
		//"label" "#GameUI_GameMenu_PlayDemo"
		//"command" "OpenLoadDemoDialog"
	//}
	"13"
	{
		"label" ""
		"command" ""
	}
	"14"
	{
		"label" "#GameUI_GameMenu_ChangeGame"
		"command" "OpenChangeGameDialog"
		"notsteam" "1"
	}
	"15"
	{
		"label" "#GameUI_GameMenu_Options"
		"command" "OpenOptionsDialog"
	}
	"16"
	{
		"label" "#GameUI_GameMenu_Quit"
		"command" "Quit"
	}
}
