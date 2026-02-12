// Copyright Epic Games, Inc. All Rights Reserved.

#include "NakamaMultiplayerSessions.h"

#define LOCTEXT_NAMESPACE "FNakamaMultiplayerSessionsModule"

void FNakamaMultiplayerSessionsModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
}

void FNakamaMultiplayerSessionsModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FNakamaMultiplayerSessionsModule, NakamaMultiplayerSessions)