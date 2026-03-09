// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class NakamaBackendTarget : TargetRules
{
    public NakamaBackendTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;

        DefaultBuildSettings = BuildSettingsVersion.V6;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_7;

        bOverrideBuildEnvironment = true;

        ExtraModuleNames.Add("NakamaBackend");
    }
}