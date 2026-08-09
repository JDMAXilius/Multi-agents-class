// Copyright Zollpa LLC.


#include "ZoransConfiguratorFunctionLibrary.h"
#include "Kismet/KismetSystemLibrary.h"

void UZoransConfiguratorFunctionLibrary::SetFSR(UObject* WorldContextObject, bool bFSREnabled)
{
    FString command = bFSREnabled ? "1" : "0";
    UKismetSystemLibrary::ExecuteConsoleCommand(WorldContextObject, "r.ForwardShading " + command);
    SetCustomCVar("ZoransSettings.ini", "RenderingSettings", "r.ForwardShading", command);
}

void UZoransConfiguratorFunctionLibrary::ToggleFSR(UObject* WorldContextObject)
{
    SetFSR(WorldContextObject, !IsFSREnabled());
}

bool UZoransConfiguratorFunctionLibrary::IsFSREnabled()
{
    FString val = "";
    GetCVar("ZoransSettings.ini", "RenderingSettings", "r.ForwardShading", val);
    return val.Contains("1");
}

void UZoransConfiguratorFunctionLibrary::SetCustomCVar(const FString& IniPath, const FString& SettingsPath, const FString& Key, const FString& Value)
{
    FConfigFile SettingsIni;

    // Set the custom setting in the INI file
    SettingsIni.SetString(*SettingsPath, *Key, *Value);

    // Save the INI file
    SettingsIni.Write(IniPath);
}

void UZoransConfiguratorFunctionLibrary::GetCVar(const FString& IniPath, const FString& SettingsPath, const FString& Key, FString& Value)
{
    FConfigFile SettingsIni;

    // Load your custom INI file
    SettingsIni.Read(IniPath);
    SettingsIni.SetString(*SettingsPath, *Key, *Value);
}