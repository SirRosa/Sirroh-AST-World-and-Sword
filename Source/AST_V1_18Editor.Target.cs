// Fill out your copyright notice in the Description page of Project Settings.

using UnrealBuildTool;
using System.Collections.Generic;

public class AST_V1_18EditorTarget : TargetRules
{
	public AST_V1_18EditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V7;

		ExtraModuleNames.AddRange( new string[] { "AST_V1_18", "Sirroh_Reimagineg_v3" } );
	}
}
