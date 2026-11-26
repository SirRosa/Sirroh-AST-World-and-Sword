#include "Items/SirrohCarryProps.h"
#include "Character/SirrohPoseBridgeComponent.h"
#include "Combat/DirectionalCombatComponent.h"
#include "Components/WidgetComponent.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Widgets/SWidget.h"
#include "Placeholder/PlaceholderHandsComponent.h"
#include "Sword/SwordWorldSubsystem.h"
#include "Items/SirrohASTInventoryComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Animation/AnimSequence.h"
#include "ASTFireReactiveComponent.h"
#include "NiagaraComponent.h"
#include "ProceduralMeshComponent.h"
#include "ASTFireFreeFlameComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "PhysicsEngine/BodySetup.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "PWG/PWGGameMode.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSirrohCarryPropsTest,"Sirroh.Items.CarryProps",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FSirrohCarryPropsTest::RunTest(const FString& Parameters)
{
    const auto Init=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false);
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Init);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    // Reproduce the POI mode: no world generator/save exists, but Loot must be ready.
    UGameInstance* GameInstance=NewObject<UGameInstance>(GEngine);
    World->SetGameInstance(GameInstance);
    FURL URL;URL.AddOption(TEXT("game=/Game/POIKit/BP_POITest_PWG.BP_POITest_PWG_C"));
    TestTrue(TEXT("POI test game mode loads"),World->SetGameMode(URL));
    auto* Mode=World->GetAuthGameMode<APWGGameMode>();
    TestTrue(TEXT("Actual POI Blueprint disables persistent generation"),Mode && Mode->bPOITestMode);
    APawn* Probe=World->SpawnActor<APawn>();
    auto* ProbeInventory=NewObject<USirrohASTInventoryComponent>(Probe);
    Probe->AddInstanceComponent(ProbeInventory);ProbeInventory->RegisterComponent();
    Probe->DispatchBeginPlay();
    TestTrue(TEXT("POI pawn inventory is ready before Loot without a generator"),ProbeInventory->IsNativeInventoryReady());
    Probe->Destroy();
    AActor* Holder=World->SpawnActor<AActor>();
    auto* Inventory=NewObject<USirrohASTInventoryComponent>(Holder);
    Holder->AddInstanceComponent(Inventory);Inventory->RegisterComponent();
    auto* Hands=NewObject<UPlaceholderHandsComponent>(Holder);
    Holder->AddInstanceComponent(Hands);Hands->RegisterComponent();
    auto* CharacterMesh=NewObject<USkeletalMeshComponent>(Holder);
    Holder->AddInstanceComponent(CharacterMesh);Holder->SetRootComponent(CharacterMesh);
    CharacterMesh->SetSkeletalMeshAsset(LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin")));
    CharacterMesh->RegisterComponent();CharacterMesh->RefreshBoneTransforms();
    auto* Combat=NewObject<UDirectionalCombatComponent>(Holder);
    Combat->bUseMotionCoreV3=true;
    Holder->AddInstanceComponent(Combat);Combat->RegisterComponent();
    Holder->DispatchBeginPlay();
    TestTrue(TEXT("Both combat grips prepared before first pickup"),Combat->IsMotionRigPrepared());
    for(const TCHAR* Kind:{TEXT("Torch"),TEXT("Box"),TEXT("Barrel"),TEXT("POI_Light_Template")})
    {
        const FString Name=FString(Kind)==TEXT("POI_Light_Template")?TEXT("BP_POI_Light_Template"):FString(TEXT("BP_AST_"))+Kind;
        const FString Path=FString::Printf(TEXT("/Game/POIKit/CarryProps/%s.%s_C"),*Name,*Name);
        UClass* Class=LoadClass<ASirrohWorldItem>(nullptr,*Path);
        if(!TestNotNull(*Path,Class))continue;
        auto* Item=World->SpawnActor<ASirrohWorldItem>(Class,FVector(0,0,100),FRotator::ZeroRotator);
        if(!TestNotNull(TEXT("Imported actor spawns"),Item))continue;
        Item->DispatchBeginPlay();
        TestNotNull(TEXT("Original AST prompt component exists"),Item->PickupPrompt.Get());
        TSharedPtr<SWidget> PromptSlate;
        if(auto* Widget=Item->PickupPrompt?Item->PickupPrompt->GetUserWidgetObject():nullptr)PromptSlate=Widget->TakeWidget();
        Item->SetPickupPromptVisible(true);
        TestTrue(TEXT("Selected item shows AST screen-space icon"),Item->PickupPrompt && Item->PickupPrompt->IsVisible() && Item->PickupPrompt->GetWidgetSpace()==EWidgetSpace::Screen && Item->PickupPrompt->GetWidgetClass());
        if(auto* Widget=Item->PickupPrompt?Item->PickupPrompt->GetUserWidgetObject():nullptr)
        {
            TestTrue(TEXT("AST widget itself is visible, not only its component"),Widget->IsVisible());
            auto* Key=Widget->WidgetTree?Widget->WidgetTree->FindWidget(TEXT("InteractionText")):nullptr;
            TestTrue(TEXT("AST interaction key is visible"),Key && Key->IsVisible());
        }
        else AddError(TEXT("AST interaction widget was not instantiated"));
        auto* Carry=Item->FindComponentByClass<USirrohCarryPoseComponent>();
        auto* Mesh=Cast<UStaticMeshComponent>(Item->GetRootComponent());
        TestNotNull(TEXT("Physical mesh is actor root"),Mesh);
        TestTrue(TEXT("Mesh has simple collision"),Mesh && Mesh->GetStaticMesh() && Mesh->GetStaticMesh()->GetBodySetup()->AggGeom.GetElementCount()>0);
        TestTrue(TEXT("Three authored overlay states assigned"),Carry && Carry->StandingIdle && Carry->StandingMove && Carry->Crouching);
        if(Carry && Carry->StandingIdle)
        {
            TestFalse(TEXT("Imported overlay contains absolute local poses"),Carry->StandingIdle->IsValidAdditive());
            TMap<FName,FTransform> Pose;Carry->ApplyPose(CharacterMesh,Pose,1.f);
            TestTrue(TEXT("Overlay samples the gripping hand"),Pose.Contains(Carry->GripBone));
            TestFalse(TEXT("Overlay preserves locomotion legs"),Pose.Contains(TEXT("thigh_r")));
            if(Carry->bTwoHanded)TestTrue(TEXT("Two-handed overlay drives both hands"),Pose.Contains(TEXT("hand_r")) && Pose.Contains(TEXT("hand_l")));
            for(const auto& Bone:Pose)TestFalse(TEXT("Sampled transform is finite"),Bone.Value.ContainsNaN());
            if(const FTransform* Hand=Pose.Find(Carry->GripBone))
            {
                const FTransform Held=Carry->HeldTransform(*Hand);
                if(auto* Torch=Cast<ASirrohTorch>(Item))
                    TestTrue(TEXT("Mirrored torch flame is above the hand"),Held.TransformPosition(Torch->FlameTip).Z>Hand->GetLocation().Z+20);
                if(Carry->bTwoHanded && Mesh && Mesh->GetStaticMesh())
                {
                    const FBox Bounds=Mesh->GetStaticMesh()->GetBoundingBox().ExpandBy(15);
                    for(FName Bone:{FName(TEXT("hand_l")),FName(TEXT("hand_r"))})
                        if(const FTransform* Other=Pose.Find(Bone))
                        {
                            const FVector Local=Held.InverseTransformPosition(Other->GetLocation());
                            AddInfo(FString::Printf(TEXT("%s %s contact %s"),Kind,*Bone.ToString(),*Local.ToString()));
                            TestTrue(*FString::Printf(TEXT("%s %s contacts carried prop bounds"),Kind,*Bone.ToString()),Bounds.IsInsideOrOn(Local));
                        }
                }
            }
        }
        if(auto* Torch=Cast<ASirrohTorch>(Item))
        {
            const auto Geometry=Torch->GetCombatDefinition();
            TestFalse(TEXT("Torch is one-handed"),Geometry.bHasSecondaryGrip);
            TestTrue(TEXT("Combat tip matches imported model"),Geometry.BladeTipLocal.Equals(FVector(0,0,47)));
            TestTrue(TEXT("Point fire only; no whole-object burn"),Torch->Fire->Coverage==EASTFireCoverage::Point && Torch->Fire->FlamePoints==1 && !Torch->Fire->bEnableSurfaceFire && Torch->Fire->bEnableProceduralFlames);
            TestTrue(TEXT("Niagara is enabled permanently"),Torch->Fire->bEnableNiagara && !Torch->Fire->bAutoExtinguish && Torch->Fire->FireSystem.LoadSynchronous());
            TestTrue(TEXT("Flame source is at tip"),Torch->Fire->PointOffset.IsNearlyZero() && Torch->FlameHead->GetRelativeLocation().Equals(Torch->FlameTip));
            TestNotNull(TEXT("Existing fire renderer is instantiated"),Torch->FindComponentByClass<UASTFireFreeFlameComponent>());
            TestTrue(TEXT("Flame source fits within tip"),Torch->FlameHead->Bounds.SphereRadius<6);
            auto* Flame=Torch->FindComponentByClass<UASTFireFreeFlameComponent>();
            TestTrue(TEXT("Point source generates flame tongues without collision"),Flame && Flame->GetTongueCount()>0);
            if(Flame)for(int32 Frame=0;Frame<60;++Frame)
                static_cast<UActorComponent*>(Flame)->TickComponent(1.f/60,LEVELTICK_All,nullptr);
            bool HasVisibleFlameGeometry=false;
            for(auto* Part:Torch->GetComponents())
                if(auto* GeometryMesh=Cast<UProceduralMeshComponent>(Part);GeometryMesh && GeometryMesh->IsVisible())
                    if(auto* Section=GeometryMesh->GetProcMeshSection(0))
                        HasVisibleFlameGeometry|=Section->ProcVertexBuffer.Num()>0 && Section->ProcIndexBuffer.Num()>0;
            TestTrue(TEXT("Fire generates visible flame triangles, not light alone"),HasVisibleFlameGeometry);
            int32 Lights=0,Emitters=0;
            for(auto* Part:Item->GetComponents())
            {
                if(auto* Light=Cast<UPointLightComponent>(Part))
                {++Lights;TestTrue(TEXT("Every light belongs to fire at the tip"),Light->ComponentHasTag(TEXT("ASTFireOwnedLight")) && Light->GetAttachParent()==Torch->FlameHead && Light->GetRelativeLocation().IsNearlyZero());}
                if(auto* FX=Cast<UNiagaraComponent>(Part))
                {++Emitters;TestTrue(TEXT("Niagara is attached only to tip"),FX->GetAttachParent()==Torch->FlameHead && FX->GetRelativeLocation().IsNearlyZero());}
            }
            TestEqual(TEXT("Exactly one fire-owned light"),Lights,1);
            TestEqual(TEXT("Exactly one localized Niagara system"),Emitters,1);
        }
        auto* Registry=World->GetSubsystem<USwordWorldSubsystem>();
        Registry->Items.Add(Item);
        for(float Depth:{-1500.f,0.f,1500.f})
        {
            Item->SetActorLocation(FVector(30,Depth,40));Item->LaneY=Depth;
            TestEqual(TEXT("Loot selector finds visible prop regardless of depth"),Hands->GetPickupCandidate(),Item);
        }
        Item->SetActorLocation(FVector(Hands->Reach+50,0,0));
        TestNull(TEXT("Loot still rejects props outside screen-space reach"),Hands->GetPickupCandidate());
        Registry->Items.RemoveAll([Item](auto Entry){return Entry.Get()==Item;});
        Item->SetActorLocation(FVector(0,0,100));Item->LaneY=0;
        TestTrue(TEXT("AST pickup can reserve item"),Item->ReservePickup(Holder));
        TestTrue(TEXT("AST contact notify can commit item"),Item->CommitPickup(Holder));
        const int32 Slot=Carry?Carry->HandSlot():0;
        TestTrue(TEXT("Same instance enters native inventory"),Inventory->SetNativeItem(Slot,Item));
        if(auto* Torch=Cast<ASirrohTorch>(Item))
        {
            auto* Bridge=Holder->FindComponentByClass<USirrohPoseBridgeComponent>();
            TMap<FName,FTransform> Pose;Carry->ApplyPose(CharacterMesh,Pose,1.f);
            if(TestNotNull(TEXT("Final-pose bridge exists"),Bridge))
            {
                Bridge->PublishHandPose(Pose);
                // Reproduce AST's attachment to the animation source before carry takes over.
                Torch->AttachToComponent(CharacterMesh,FAttachmentTransformRules::KeepWorldTransform,TEXT("hand_r"));
                Bridge->TickComponent(1.f/60,LEVELTICK_All,nullptr);
                FTransform Index,Middle,Pinky;
                const bool Palm=Bridge->TryGetFinalBoneTransform(TEXT("index_01_r"),Index)
                    && Bridge->TryGetFinalBoneTransform(TEXT("middle_02_r"),Middle)
                    && Bridge->TryGetFinalBoneTransform(TEXT("pinky_01_r"),Pinky);
                TestTrue(TEXT("Torch handle contacts rendered palm, not forearm"),Palm && Torch->GetActorLocation().Equals((Index.GetLocation()+Middle.GetLocation()+Pinky.GetLocation())/3.0,.01));
                TestNull(TEXT("Hidden animation source cannot move held torch"),Torch->GetAttachParentActor());
            }
        }
        TestFalse(TEXT("Pickup hides interaction icon"),Item->PickupPrompt && Item->PickupPrompt->IsVisible());
        TestTrue(TEXT("Held item has no collision"),Mesh && Mesh->GetCollisionEnabled()==ECollisionEnabled::NoCollision);
        const auto Saved=Inventory->CaptureNativeItems();
        TestTrue(TEXT("Save retains imported class"),Saved.Num()==1 && !Saved[0].CarryClass.IsNull());
        TestFalse(TEXT("Prop cannot be swapped into backpack"),Inventory->SwapNativeItems(Slot,2));
        if(auto* Torch=Cast<ASirrohTorch>(Item))
        {
            TestTrue(TEXT("Torch swaps to the left hand"),Inventory->SwapNativeItems(0,1));
            TestEqual(TEXT("Left torch uses original GASP left hand"),Torch->CarryPose->GripBone,FName(TEXT("hand_l")));
            TestFalse(TEXT("Left torch uses unmirrored authored pose"),Torch->CarryPose->bMirrorPose);
            TestTrue(TEXT("Torch swaps back to combat hand"),Inventory->SwapNativeItems(0,1));
            TestEqual(TEXT("Right torch restores right hand"),Torch->CarryPose->GripBone,FName(TEXT("hand_r")));
        }
        Inventory->ThrowNativeItem(Slot,FVector(0,0,100),FVector(250,0,150));
        TestNull(TEXT("Q throw clears inventory slot"),Inventory->GetNativeItem(Slot));
        TestFalse(TEXT("Thrown instance is no longer held"),Item->IsHeld());
        TestTrue(TEXT("Throw enables physics"),Mesh && Mesh->IsSimulatingPhysics());
        Item->Destroy();
    }
    GEngine->DestroyWorldContext(World);World->DestroyWorld(false);
    return true;
}
#endif
