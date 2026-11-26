// Fill out your copyright notice in the Description page of Project Settings.


using UnrealBuildTool;


public class Sirroh_Reimagineg_v3 : ModuleRules
{
        public Sirroh_Reimagineg_v3(ReadOnlyTargetRules Target) : base(Target)
        {
                PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
                if (Target.bBuildEditor) PrivateDependencyModuleNames.AddRange(new string[] { "UnrealEd", "Kismet", "KismetCompiler", "BlueprintGraph" });
        
                PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "DeveloperSettings", "InputCore", "EnhancedInput", "ProceduralMeshComponent", "AssetRegistry", "Mover", "MeshDescription", "StaticMeshDescription", "MeshConversion" });


                PublicDependencyModuleNames.AddRange(new string[] { "ControlRig", "RigVM", "SlateCore", "Niagara", "ASTFireRuntime" });
                PrivateDependencyModuleNames.AddRange(new string[] { "Json", "JsonUtilities", "Slate", "UMG", "AnimationCore", "DragonIKPlugin", "GameplayTags", "PhysicsCore", "PhysicsControl", "Chooser", "AIModule", "NavigationSystem", "MotionWarping" });
                
                // Uncomment if you are using online features
                // PrivateDependencyModuleNames.Add("OnlineSubsystem");


                // To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
        }
}
