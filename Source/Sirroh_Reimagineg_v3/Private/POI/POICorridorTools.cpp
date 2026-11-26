#include "POI/POICorridorTools.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"
#include "Combat/CombatVitalityComponent.h"
#include "POI/POIPushTraversalComponent.h"
#include "POI/POIPhysicalPushComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Blueprint.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"
#if WITH_EDITOR
#include "K2Node_Self.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_IfThenElse.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/InheritableComponentHandler.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Misc/PackageName.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "StaticMeshAttributes.h"
#include "PhysicsEngine/BodySetup.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Message.h"
#include "K2Node_VariableGet.h"
#include "EdGraphSchema_K2.h"
#endif

APOITorch::APOITorch()
{
    PrimaryActorTick.bCanEverTick=false;
    Mesh=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TorchMesh")); RootComponent=Mesh;
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Asset(TEXT("/Game/ZL_retroCaves/Meshes/decos/m_deco_cave_torch.m_deco_cave_torch"));
    Mesh->SetStaticMesh(Asset.Object); Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Light=CreateDefaultSubobject<UPointLightComponent>(TEXT("TorchLight")); Light->SetupAttachment(Mesh);
    Light->SetMobility(EComponentMobility::Movable); Light->SetIntensity(1500);
    Light->SetLightColor(FLinearColor(1,.48f,.15f)); Light->SetAttenuationRadius(650);
    Light->SetCastShadows(false);
    const float Top=Asset.Object?Asset.Object->GetBoundingBox().Max.Z:100;
    Light->SetRelativeLocation(FVector(0,-20,Top));
}

UBlueprint* UPOICorridorTools::CreatePushableVariant(const FString& PackagePath,UStaticMesh* NewMesh)
{
#if WITH_EDITOR
    if (!NewMesh || !FPackageName::IsValidLongPackageName(PackagePath)) return nullptr;
    auto* Existing=LoadObject<UBlueprint>(nullptr,*PackagePath);
    auto* Parent=LoadObject<UBlueprint>(nullptr,TEXT("/AST_InteractionSystem/Actors/PullPushObject.PullPushObject"));
    if (!Parent || !Parent->GeneratedClass || !Parent->SimpleConstructionScript) return nullptr;
    auto* BP=Existing?Existing:FKismetEditorUtilities::CreateBlueprint(Parent->GeneratedClass,CreatePackage(*PackagePath),FName(*FPackageName::GetShortName(PackagePath)),BPTYPE_Normal,UBlueprint::StaticClass(),UBlueprintGeneratedClass::StaticClass());
    if (!BP) return nullptr;
    auto* Handler=BP->GetInheritableComponentHandler(true);
    const FBox Bounds=NewMesh->GetBoundingBox();
    const FVector Center=Bounds.GetCenter(), Size=Bounds.GetSize();
    const float HandZ=Bounds.Min.Z+FMath::Min(100.f,Size.Z);
    for (USCS_Node* Node:Parent->SimpleConstructionScript->GetAllNodes())
    {
        const FString Name=Node->GetVariableName().ToString();
        auto* Original=Cast<USceneComponent>(Node->ComponentTemplate);
        if (!Original) continue;
        auto* Override=Cast<USceneComponent>(Handler->CreateOverridenComponentTemplate(FComponentKey(Node)));
        if (!Override) continue;
        if (Name==TEXT("PullPushObject"))
        {
            auto* Body=CastChecked<UStaticMeshComponent>(Override);
            Body->SetStaticMesh(NewMesh); Body->EmptyOverrideMaterials();
            if(!Body->BodyInstance.bOverrideMass)
                Body->BodyInstance.SetMassOverride(PackagePath.EndsWith(TEXT("Bench"))?25.f:(PackagePath.EndsWith(TEXT("Barrel"))?40.f:60.f),true);
            Body->BodyInstance.bLockYTranslation=true;
            Body->BodyInstance.bLockXRotation=true; Body->BodyInstance.bLockZRotation=true;
            Body->BodyInstance.DOFMode=EDOFMode::SixDOF;
        }
        else if (Name==TEXT("FrontActor") || Name==TEXT("BackActor") || Name==TEXT("LeftActor") || Name==TEXT("RightActor"))
        {
            const FVector Old=Original->GetRelativeLocation();
            Override->SetRelativeLocation(FVector(Center.X+Old.X*Size.X/100,Center.Y+Old.Y*Size.Y/100,HandZ));
        }
        else if (Name.EndsWith(TEXT("WarpPoint")))
        {
            FVector P=Original->GetRelativeLocation(); P.Z=Bounds.Min.Z-HandZ;
            Override->SetRelativeLocation(P);
        }
        else if (Name.StartsWith(TEXT("Obje")) && Name.EndsWith(TEXT("Trigger")))
        {
            FVector P=Original->GetRelativeLocation(); P.Z=Bounds.Min.Z-HandZ+35;
            Override->SetRelativeLocation(P);
        }
        else if (Name.StartsWith(TEXT("ParkourActor")))
        {
            const FVector Old=Original->GetRelativeLocation();
            Override->SetRelativeLocation(FVector(Center.X+Old.X*Size.X/100,Center.Y+Old.Y*Size.Y/100,Bounds.Max.Z));
            FVector Scale=Original->GetRelativeScale3D(); Scale.X*=FMath::Abs(Old.X)>40?Size.Y/100:Size.X/100;
            Override->SetRelativeScale3D(Scale);
        }
    }
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
    FKismetEditorUtilities::CompileBlueprint(BP);
    if (!Existing) FAssetRegistryModule::AssetCreated(BP); BP->MarkPackageDirty(); return BP;
#else
    return nullptr;
#endif
}

bool UPOICorridorTools::ShouldReleasePhysicalPush(AActor* Object)
{
 auto* Drive=Object?Object->FindComponentByClass<UPOIPhysicalPushComponent>():nullptr;
 return !Drive || !Drive->IsWithinReach();
}
bool UPOICorridorTools::ApplyPropStrike(UPrimitiveComponent* Body,FVector Velocity,float WeaponMass)
{
    if(!Body || !Body->IsSimulatingPhysics() || !Body->GetOwner()->HasAuthority() || Velocity.ContainsNaN())return false;
    Velocity.Y=0;
    if(Velocity.Size()<100)return false;
    Body->AddImpulse((Velocity*FMath::Max(.1f,WeaponMass)*.5f).GetClampedToMaxSize(FMath::Min(6000.,double(Body->GetMass())*100.)));
    return true;
}
void UPOICorridorTools::PreparePhysicalPush(AActor* Object)
{
    if(!Object)return;
    auto* Drive=Object->FindComponentByClass<UPOIPhysicalPushComponent>();
    if(!Drive){Drive=NewObject<UPOIPhysicalPushComponent>(Object);Object->AddInstanceComponent(Drive);Drive->RegisterComponent();}
    Drive->CaptureBeforeGrab();
}
void UPOICorridorTools::BeginPhysicalPush(AActor* Object)
{
    if (!Object) return;
    auto* Drive=Object->FindComponentByClass<UPOIPhysicalPushComponent>();
    if (!Drive) { Drive=NewObject<UPOIPhysicalPushComponent>(Object);Object->AddInstanceComponent(Drive);Drive->RegisterComponent(); }
    Drive->StartDrive();
}
AActor* UPOICorridorTools::GetHeldPushable(AActor* Character)
{
    if (!Character) return nullptr;
    for (UActorComponent* C:Character->GetComponents()) if (C && C->GetClass()->GetName().StartsWith(TEXT("BPC_Interaction")))
        if (auto* P=FindFProperty<FObjectPropertyBase>(C->GetClass(),TEXT("InteractingActor")))
            if (auto* A=Cast<AActor>(P->GetObjectPropertyValue_InContainer(C)); A && A->FindFunction(TEXT("Get PullPush IK Location")))
            {
                if (auto* Drive=A->FindComponentByClass<UPOIPhysicalPushComponent>();Drive && Drive->IsDrivenBy(Character)) return A;
                if (auto* Parent=A->GetRootComponent()?A->GetRootComponent()->GetAttachParent():nullptr; Parent && Parent->GetOwner()==Character) return A;
            }
    return nullptr;
}
void UPOICorridorTools::ConfigurePushPlane(AActor* Object)
{
    if (auto* Body=Object?Cast<UPrimitiveComponent>(Object->GetRootComponent()):nullptr)
    {
        auto& BI=Body->BodyInstance;
        const bool Changed=!BI.bLockYTranslation || !BI.bLockXRotation || !BI.bLockZRotation || BI.DOFMode!=EDOFMode::SixDOF;
        BI.bLockYTranslation=true; BI.bLockXRotation=true; BI.bLockZRotation=true;
        if (Changed) Body->SetConstraintMode(EDOFMode::SixDOF);
    }
}
bool UPOICorridorTools::ReleaseHeldPushable(AActor* Character)
{
    AActor* Object=GetHeldPushable(Character); if (!Object) return false;
    for (UActorComponent* C:Character->GetComponents()) if (C && C->GetClass()->GetName().StartsWith(TEXT("BPC_Interaction")))
        if (UFunction* F=C->FindFunction(TEXT("Release PullPush Object")))
        {
            FStructOnScope Args(F);
            if (auto* P=FindFProperty<FObjectPropertyBase>(F,TEXT("New Relased Actor")))
            {
                P->SetObjectPropertyValue_InContainer(Args.GetStructMemory(),Object);
                C->ProcessEvent(F,Args.GetStructMemory());
                ConfigurePushPlane(Object);
                return GetHeldPushable(Character)==nullptr;
            }
        }
    return false;
}

static FVector PushExtent(AActor* Object)
{
    if (Object && Object->FindFunction(TEXT("Get PullPush IK Location")))
        if (auto* Body=Cast<UStaticMeshComponent>(Object->GetRootComponent()))
            return Body->CalcBounds(Body->GetComponentTransform()).BoxExtent;
    return FVector(50);
}
FVector UPOICorridorTools::PushSupportEnd(AActor* Object,FVector Start) { return Start-FVector(0,0,PushExtent(Object).Z+35.f); }
FVector UPOICorridorTools::PushTraceExtent(AActor* Object) { return (PushExtent(Object)-FVector(4,4,2)).ComponentMax(FVector(5)); }
FVector UPOICorridorTools::PushProxyExtent(AActor* Object) { return (PushExtent(Object)+FVector(2,2,-8)).ComponentMax(FVector(5)); }
FVector UPOICorridorTools::PushProxyStart(AActor* Object) { const FVector E=PushExtent(Object); return FVector(E.X+60,0,E.Z-95); }
bool UPOICorridorTools::PushFloorTrace(const UObject* Context,FVector Start,FVector End,FVector HalfSize,FRotator Orientation,ETraceTypeQuery Channel,bool Complex,const TArray<AActor*>& Ignored,EDrawDebugTrace::Type Debug,FHitResult& Hit,bool IgnoreSelf,FLinearColor Color,FLinearColor HitColor,float Time)
{
    TArray<AActor*> Excluded=Ignored;
    const AActor* Owner=Cast<AActor>(Context);
    if (const auto* Component=Cast<UActorComponent>(Context)) Owner=Component->GetOwner();
    if (Owner && IgnoreSelf) Excluded.Add(const_cast<AActor*>(Owner));
    const auto Roots=Excluded;
    for (AActor* A:Roots) if (A)
    {
        TArray<AActor*> Parts; A->GetAllChildActors(Parts,true); Excluded.Append(Parts);
        A->GetAttachedActors(Parts,true,true); Excluded.Append(Parts);
    }
    const bool Found=UKismetSystemLibrary::BoxTraceSingle(Context,Start,End,HalfSize,Orientation,Channel,Complex,Excluded,Debug,Hit,IgnoreSelf,Color,HitColor,Time);
    if (Found && !Hit.bStartPenetrating && Hit.ImpactNormal.Z>.5f) return true;
    // Initial overlap is not a new floor height. AST adds a clearance to Hit.Location;
    // feeding it a penetrating start point causes a repeated upward displacement.
    UWorld* World=GEngine->GetWorldFromContextObject(Context,EGetWorldErrorMode::ReturnNull);
    if (!World) return false;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(ASTPushSupport),Complex); Query.AddIgnoredActors(Excluded);
    FHitResult Floor;
    if (!World->LineTraceSingleByChannel(Floor,Start,End-FVector(0,0,HalfSize.Z+100),UEngineTypes::ConvertToCollisionChannel(Channel),Query) || Floor.bStartPenetrating || Floor.ImpactNormal.Z<=.5f) return false;
    Hit=Floor; Hit.Location=Floor.ImpactPoint+FVector(0,0,HalfSize.Z); return true;
}
FVector UPOICorridorTools::CenterPushMesh(UStaticMesh* Mesh)
{
#if WITH_EDITOR
    if (!Mesh || !Mesh->GetPathName().StartsWith(TEXT("/Game/POIKit/Underground/Meshes/"))) return FVector::ZeroVector;
    const FVector Center=Mesh->GetBoundingBox().GetCenter();
    if (Center.IsNearlyZero(.01f)) return FVector::ZeroVector;
    Mesh->Modify();
    for (int32 LOD=0;LOD<Mesh->GetNumSourceModels();++LOD) if (auto* Description=Mesh->GetMeshDescription(LOD))
    {
        FStaticMeshAttributes Attributes(*Description); auto Positions=Attributes.GetVertexPositions();
        for (auto Vertex:Description->Vertices().GetElementIDs()) Positions[Vertex]-=FVector3f(Center);
        Mesh->CommitMeshDescription(LOD);
    }
    if (auto* Body=Mesh->GetBodySetup())
    {
        Body->Modify();
        for (auto& Box:Body->AggGeom.BoxElems) Box.Center-=Center;
        for (auto& Convex:Body->AggGeom.ConvexElems) { for (auto& V:Convex.VertexData) V-=Center; Convex.UpdateElemBox(); }
        Body->InvalidatePhysicsData(); Body->CreatePhysicsMeshes();
    }
    Mesh->Build(false); Mesh->MarkPackageDirty(); return Center;
#else
    return FVector::ZeroVector;
#endif
}

bool UPOICorridorTools::PatchASTPushGeometry()
{
#if WITH_EDITOR
    auto* BP=LoadObject<UBlueprint>(nullptr,TEXT("/AST_InteractionSystem/Blueprints/ActorComponent/BPC_Interaction.BPC_Interaction"));
    if (!BP) return false;
    BP->Modify(); const auto* Schema=GetDefault<UEdGraphSchema_K2>();
    TArray<UEdGraph*> Graphs; BP->GetAllGraphs(Graphs);
    for (auto* Graph:Graphs)
    {
        const FString Name=Graph->GetName();
        if(Name==TEXT("CalculateBoxTransform"))
        {
            UEdGraphNode* Release=nullptr;
            UK2Node_CallFunction* Guard=nullptr;
            for(UEdGraphNode* N:Graph->Nodes)
            {
                if(N->NodeGuid.ToString().StartsWith(TEXT("71EAFC73")))Release=N;
                if(auto* C=Cast<UK2Node_CallFunction>(N);C && C->FunctionReference.GetMemberName()==GET_FUNCTION_NAME_CHECKED(UPOICorridorTools,ShouldReleasePhysicalPush))Guard=C;
            }
            if(Release && !Guard)
            {
                Guard=NewObject<UK2Node_CallFunction>(Graph);Graph->AddNode(Guard);Guard->CreateNewGuid();
                Guard->SetFromFunction(StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UPOICorridorTools,ShouldReleasePhysicalPush)));Guard->AllocateDefaultPins();
                auto* Branch=NewObject<UK2Node_IfThenElse>(Graph);Graph->AddNode(Branch);Branch->CreateNewGuid();Branch->AllocateDefaultPins();
                auto* Exec=Release->FindPinChecked(TEXT("execute"));const auto Links=Exec->LinkedTo;Exec->BreakAllPinLinks();
                for(auto* P:Links)Schema->TryCreateConnection(P,Branch->GetExecPin());
                for(auto* P:Release->FindPinChecked(TEXT("ReleaseActor"))->LinkedTo)Schema->TryCreateConnection(P,Guard->FindPinChecked(TEXT("Object")));
                Schema->TryCreateConnection(Guard->GetReturnValuePin(),Branch->GetConditionPin());
                Schema->TryCreateConnection(Branch->GetThenPin(),Exec);
            }
        }
        const auto Nodes=Graph->Nodes;
        for (UEdGraphNode* Node:Nodes) if (auto* Call=Cast<UK2Node_CallFunction>(Node))
        {
            const FName Function=Call->FunctionReference.GetMemberName();
            if (Name==TEXT("Update PushPullCollision TransformWhileMove") && Function==TEXT("Greater_DoubleDouble") &&
                Call->GetReturnValuePin()->LinkedTo.ContainsByPredicate([](const UEdGraphPin* Pin){return Pin->PinName==TEXT("FirstMoveWhenPush");}))
                if (auto* Threshold=Call->FindPin(TEXT("B")); Threshold && !Threshold->LinkedTo.Num() &&
                    (Threshold->DefaultValue==TEXT("225.000000") || Threshold->DefaultValue==TEXT("-1.0")))
                    Schema->TrySetDefaultValue(*Threshold,TEXT("225.000000")); // Preserve AST geometry gate during the grab montage.
            if ((Name==TEXT("CalculateBoxTransform") || Name==TEXT("Set PushPullCollision Location")) && Function==TEXT("BoxTraceSingle"))
            {
                Call->SetFromFunction(StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UPOICorridorTools,PushFloorTrace)));
                Call->ReconstructNode();
            }
            if (Name==TEXT("CalculateBoxTransform") && Function==TEXT("SphereTraceSingle"))
            {
                auto* End=Call->FindPinChecked(TEXT("End"));
                const bool Patched=End->LinkedTo.ContainsByPredicate([](const UEdGraphPin* P){auto* C=Cast<UK2Node_CallFunction>(P->GetOwningNode());return C && C->FunctionReference.GetMemberName()==GET_FUNCTION_NAME_CHECKED(UPOICorridorTools,PushSupportEnd);});
                if (!Patched)
                {
                    auto* Start=Call->FindPinChecked(TEXT("Start")); if (Start->LinkedTo.Num()!=1) return false;
                    auto* Get=NewObject<UK2Node_VariableGet>(Graph); Graph->AddNode(Get); Get->CreateNewGuid();
                    Get->VariableReference.SetSelfMember(TEXT("InteractingActor")); Get->AllocateDefaultPins();
                    auto* Support=NewObject<UK2Node_CallFunction>(Graph); Graph->AddNode(Support); Support->CreateNewGuid();
                    Support->SetFromFunction(StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UPOICorridorTools,PushSupportEnd))); Support->AllocateDefaultPins();
                    Schema->TryCreateConnection(Get->GetValuePin(),Support->FindPinChecked(TEXT("Object")));
                    Schema->TryCreateConnection(Start->LinkedTo[0],Support->FindPinChecked(TEXT("Start")));
                    End->BreakAllPinLinks(); Schema->TryCreateConnection(Support->GetReturnValuePin(),End);
                }
            }
            FName Helper; UEdGraphPin* Input=nullptr;
            if ((Name==TEXT("CalculateBoxTransform") || Name==TEXT("Set PushPullCollision Location")) && Function==TEXT("BoxTraceSingle"))
            { Helper=GET_FUNCTION_NAME_CHECKED(UPOICorridorTools,PushTraceExtent); Input=Call->FindPin(TEXT("HalfSize")); }
            if (Name==TEXT("Define Push Object Box Collision") && Function==TEXT("SetBoxExtent"))
            { Helper=GET_FUNCTION_NAME_CHECKED(UPOICorridorTools,PushProxyExtent); Input=Call->FindPin(TEXT("InBoxExtent")); }
            if (Name==TEXT("Grab PullPush Object") && Function==TEXT("K2_SetRelativeTransform"))
            { Helper=GET_FUNCTION_NAME_CHECKED(UPOICorridorTools,PushProxyStart); Input=Call->FindPin(TEXT("NewTransform_Location")); }
            if (!Input || Input->LinkedTo.Num()) continue;
            auto* Get=NewObject<UK2Node_VariableGet>(Graph); Graph->AddNode(Get); Get->CreateNewGuid();
            Get->VariableReference.SetSelfMember(TEXT("InteractingActor")); Get->AllocateDefaultPins();
            auto* Geometry=NewObject<UK2Node_CallFunction>(Graph); Graph->AddNode(Geometry); Geometry->CreateNewGuid();
            Geometry->SetFromFunction(StaticClass()->FindFunctionByName(Helper)); Geometry->AllocateDefaultPins();
            Schema->TryCreateConnection(Get->GetValuePin(),Geometry->FindPinChecked(TEXT("Object")));
            Schema->TryCreateConnection(Geometry->GetReturnValuePin(),Input);
        }
        // The approach montage can already enable hand IK before the grab notify.
        // Refresh AST's world-space targets before that FIRST montage, not only StartPullPush.
        if (Name==TEXT("Checking Interaction PullPushObje"))
        {
            UK2Node_CallFunction *Before=nullptr,*Approach=nullptr,*IK=nullptr;
            for (UEdGraphNode* N:Graph->Nodes) if (auto* C=Cast<UK2Node_CallFunction>(N))
            {
                const FName F=C->FunctionReference.GetMemberName();
                if (F==TEXT("FindMontageStartTime")) Before=C;
                if (F==TEXT("Play MoveToInteractionMontage")) Approach=C;
                if (F==TEXT("Set  Interaction  IK")) IK=C;
            }
            if (!Before || !Approach) return false;
            if (!IK)
            {
                UFunction* IKFunction=nullptr;
                for (UEdGraph* Other:Graphs) for (UEdGraphNode* N:Other->Nodes)
                    if (auto* C=Cast<UK2Node_CallFunction>(N); C && C->FunctionReference.GetMemberName()==TEXT("Set  Interaction  IK")) IKFunction=C->GetTargetFunction();
                if (!IKFunction) return false;
                IK=NewObject<UK2Node_Message>(Graph); Graph->AddNode(IK); IK->CreateNewGuid();
                IK->SetFromFunction(IKFunction); IK->AllocateDefaultPins();
                auto* State=NewObject<UK2Node_VariableGet>(Graph); Graph->AddNode(State); State->CreateNewGuid();
                State->VariableReference.SetSelfMember(TEXT("StateManager")); State->AllocateDefaultPins();
                if (!Schema->TryCreateConnection(State->GetValuePin(),IK->FindPinChecked(TEXT("self")))) return false;
                Schema->TrySetDefaultValue(*IK->FindPinChecked(TEXT("InteractionIKType")),TEXT("NewEnumerator4"));
                Schema->TrySetDefaultValue(*IK->FindPinChecked(TEXT("InteractionType")),TEXT("NewEnumerator13"));
            }
            Before->FindPinChecked(TEXT("then"))->BreakAllPinLinks();
            Approach->FindPinChecked(TEXT("execute"))->BreakAllPinLinks();
            if (!Schema->TryCreateConnection(Before->FindPinChecked(TEXT("then")),IK->FindPinChecked(TEXT("execute"))) ||
                !Schema->TryCreateConnection(IK->FindPinChecked(TEXT("then")),Approach->FindPinChecked(TEXT("execute")))) return false;
        }
        if (Name==TEXT("Grab PullPush Object"))
        {
            UEdGraphNode *Position=nullptr,*Attach=nullptr,*Ignore=nullptr,*State=nullptr,*Relative=nullptr;
            for (UEdGraphNode* N:Graph->Nodes)
            {
                const FString Id=N->NodeGuid.ToString();
                if (Id.StartsWith(TEXT("FB19B9AE"))) Position=N;
                if (Id.StartsWith(TEXT("8E64A426"))) Attach=N;
                if (Id.StartsWith(TEXT("083631C7"))) Ignore=N;
                if (Id.StartsWith(TEXT("FF465115"))) State=N;
                if (Id.StartsWith(TEXT("9FE49666"))) Relative=N;
            }
            if (!Position || !Attach || !Ignore || !State || !Relative) return false;
            // The AST proxy may have a different scale from an authored prop.
            // Capturing the grip must not resize the physical body.
            Schema->TrySetDefaultValue(*Attach->FindPinChecked(TEXT("ScaleRule")),TEXT("KeepWorld"));
            if(Attach->FindPinChecked(TEXT("ScaleRule"))->DefaultValue!=TEXT("KeepWorld"))return false;
            auto Link=[&](UEdGraphNode* A,UEdGraphNode* B)
            {
                auto* Out=A->FindPinChecked(TEXT("then")); auto* In=B->FindPinChecked(TEXT("execute"));
                Out->BreakAllPinLinks(); In->BreakAllPinLinks(); return Schema->TryCreateConnection(Out,In);
            };
            // Disable simulation through AST's held state BEFORE attachment.
            if (!Link(Position,State) || !Link(State,Attach) || !Link(Ignore,Relative)) return false;
            UEdGraphNode* Last=nullptr; UK2Node_CallFunction* Drive=nullptr;
            for (UEdGraphNode* N:Graph->Nodes)
            {
                if (N->NodeGuid.ToString().StartsWith(TEXT("DF864F91"))) Last=N;
                if(auto* C=Cast<UK2Node_CallFunction>(N);C && C->FunctionReference.GetMemberName()==GET_FUNCTION_NAME_CHECKED(UPOICorridorTools,BeginPhysicalPush)) Drive=C;
            }
            if (!Last) return false;
            if (!Drive)
            {
                Drive=NewObject<UK2Node_CallFunction>(Graph);Graph->AddNode(Drive);Drive->CreateNewGuid();
                Drive->SetFromFunction(StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UPOICorridorTools,BeginPhysicalPush)));Drive->AllocateDefaultPins();
                auto* ObjectPin=Attach->FindPinChecked(TEXT("self"));
                if(ObjectPin->LinkedTo.Num()!=1 || !Schema->TryCreateConnection(ObjectPin->LinkedTo[0],Drive->FindPinChecked(TEXT("Object"))))return false;
            }
            if(!Link(Last,Drive))return false;
            UEdGraphNode *Entry=nullptr,*Define=nullptr;UK2Node_CallFunction* Prepare=nullptr;
            for(UEdGraphNode* N:Graph->Nodes)
            {
                if(N->NodeGuid.ToString().StartsWith(TEXT("74C36D87")))Entry=N;
                if(N->NodeGuid.ToString().StartsWith(TEXT("E2BB80CB")))Define=N;
                if(auto* C=Cast<UK2Node_CallFunction>(N);C && C->FunctionReference.GetMemberName()==GET_FUNCTION_NAME_CHECKED(UPOICorridorTools,PreparePhysicalPush))Prepare=C;
            }
            if(!Entry || !Define)return false;
            if(!Prepare)
            {
                Prepare=NewObject<UK2Node_CallFunction>(Graph);Graph->AddNode(Prepare);Prepare->CreateNewGuid();
                Prepare->SetFromFunction(StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UPOICorridorTools,PreparePhysicalPush)));Prepare->AllocateDefaultPins();
                auto* ObjectPin=Drive->FindPinChecked(TEXT("Object"));
                if(ObjectPin->LinkedTo.Num()!=1 || !Schema->TryCreateConnection(ObjectPin->LinkedTo[0],Prepare->FindPinChecked(TEXT("Object"))))return false;
            }
            if(!Link(Entry,Prepare) || !Link(Prepare,Define))return false;
        }
        // At rest refresh only native IK, never the geometry/relative-transform sequence.
        if (Name==TEXT("Update PushPullCollision TransformWhileMove"))
        {
            UEdGraphNode *Gate=nullptr,*LocalIK=nullptr;
            for (UEdGraphNode* N:Graph->Nodes)
            {
                if (N->NodeGuid.ToString()==TEXT("F887BB654604B214C705A2AD67D4A6D2")) Gate=N;
                if (N->NodeGuid.ToString()==TEXT("C3F22E944CE6E0847CED4C9B806734CA")) LocalIK=N;
            }
            if (!Gate || !LocalIK) return false;
            auto* Idle=Gate->FindPin(TEXT("else"));
            auto* Execute=LocalIK->FindPin(TEXT("execute"));
            if (!Idle || !Execute) return false;
            if (!Idle->LinkedTo.Contains(Execute) && !Schema->TryCreateConnection(Idle,Execute)) return false;
            // The held body is now independently simulated. These legacy
            // writes used attachment-relative coordinates on its detached root,
            // teleporting it into world space on the first movement update.
            for(UEdGraphNode* N:Graph->Nodes)
            {
                const FString Id=N->NodeGuid.ToString();
                if(!Id.StartsWith(TEXT("32B76C2D")) && !Id.StartsWith(TEXT("7752C505")))continue;
                auto* In=N->FindPinChecked(TEXT("execute"));auto* Out=N->FindPinChecked(TEXT("then"));
                const auto Previous=In->LinkedTo,Next=Out->LinkedTo;
                In->BreakAllPinLinks();Out->BreakAllPinLinks();
                for(auto* P:Previous)for(auto* Q:Next)if(!Schema->TryCreateConnection(P,Q))return false;
            }
        }
        // Establish actor + side BEFORE the montage can evaluate its first IK pose/notify.
        if (Name==TEXT("Play Pullpush Interaction"))
        {
            UEdGraphNode *Entry=nullptr,*Montage=nullptr,*Server=nullptr,*SetActor=nullptr,*SetDirection=nullptr,*IK=nullptr;
            for (UEdGraphNode* N:Graph->Nodes)
            {
                if (N->GetClass()->GetName()==TEXT("K2Node_FunctionEntry")) Entry=N;
                if (auto* C=Cast<UK2Node_CallFunction>(N))
                {
                    auto F=C->FunctionReference.GetMemberName();
                    if (F==TEXT("Play Interaction Montage")) Montage=N;
                    if (F==TEXT("ServerInteractionMontage")) Server=N;
                    if (F==TEXT("Set  Interaction  IK")) IK=N;
                }
                if (N->GetClass()->GetName()==TEXT("K2Node_VariableSet"))
                {
                    if (N->FindPin(TEXT("InteractingActor"))) SetActor=N;
                    if (N->FindPin(TEXT("Pull Push Direction"))) SetDirection=N;
                }
            }
            if (Entry && Montage && Server && SetActor && SetDirection && IK)
            {
                auto Link=[&](UEdGraphNode* A,UEdGraphNode* B)
                { auto* Out=A->FindPin(TEXT("then")); auto* In=B->FindPin(TEXT("execute")); Out->BreakAllPinLinks(); In->BreakAllPinLinks(); Schema->TryCreateConnection(Out,In); };
                Server->FindPin(TEXT("then"))->BreakAllPinLinks();
                Link(Entry,SetActor); Link(SetDirection,IK); Link(IK,Montage);
            }
        }
    }
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP); FKismetEditorUtilities::CompileBlueprint(BP); BP->MarkPackageDirty();
    if (BP->Status==BS_Error) return false;
    // Released pushables stay dynamic: Chaos sleeps them after they settle and
    // wakes them on contact/impulse. AST's linear-speed-only freeze ignored spin.
    auto* PushBP=LoadObject<UBlueprint>(nullptr,TEXT("/AST_InteractionSystem/Actors/PullPushObject.PullPushObject"));
    if (!PushBP) return false;
    TArray<UEdGraph*> PushGraphs; PushBP->GetAllGraphs(PushGraphs);
    UEdGraphNode *StateSwitch=nullptr,*HeldVisibility=nullptr,*SimOn=nullptr,*SimOff=nullptr,*ReleasedIcons=nullptr;
    for (auto* G:PushGraphs) for (UEdGraphNode* N:G->Nodes)
    {
        const FString Id=N->NodeGuid.ToString();
        if (Id.StartsWith(TEXT("4A4B774D"))) StateSwitch=N;
        if (Id.StartsWith(TEXT("53A16532"))) HeldVisibility=N;
        if (Id.StartsWith(TEXT("4CB690A0"))) SimOn=N;
        if (Id.StartsWith(TEXT("F9646B39"))) SimOff=N;
        if (Id.StartsWith(TEXT("EC47A057"))) ReleasedIcons=N;
    }
    if (!StateSwitch || !HeldVisibility || !SimOn || !SimOff || !ReleasedIcons) return false;
    PushBP->Modify();
    if (!PushBP->SimpleConstructionScript->FindSCSNode(TEXT("POIPushTraversal")))
        PushBP->SimpleConstructionScript->AddNode(PushBP->SimpleConstructionScript->CreateNode(UPOIPushTraversalComponent::StaticClass(),TEXT("POIPushTraversal")));
    auto Connect=[&](UEdGraphPin* Out,UEdGraphPin* In)
    {
        if (!Out || !In) return false;
        Out->BreakAllPinLinks(); In->BreakAllPinLinks();
        return Schema->TryCreateConnection(Out,In);
    };
    if (!Connect(StateSwitch->FindPin(TEXT("NewEnumerator0")),SimOff->FindPin(TEXT("execute"))) ||
        !Connect(SimOff->FindPin(TEXT("then")),HeldVisibility->FindPin(TEXT("execute"))) ||
        !Connect(SimOn->FindPin(TEXT("then")),ReleasedIcons->FindPin(TEXT("execute")))) return false;
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(PushBP);
    FKismetEditorUtilities::CompileBlueprint(PushBP); PushBP->MarkPackageDirty();
    return PushBP->Status!=BS_Error;
#else
    return false;
#endif
}

FText UPOICorridorTools::SafeHUDHealth(UUserWidget* Widget)
{
 auto* Pawn=IsValid(Widget)?Widget->GetOwningPlayerPawn():nullptr;
 if(!IsValid(Pawn))return FText::GetEmpty();
 if(auto* Health=Pawn->FindComponentByClass<UCombatVitalityComponent>())return FText::AsNumber(100.*Health->Blood/FMath::Max(1.f,Health->MaxBlood));
 for(auto* C:Pawn->GetComponents())if(IsValid(C) && C->ComponentHasTag(TEXT("DamageSystem")))
  if(auto* F=C->FindFunction(TEXT("Get CurrentHealth")))
  {
   FStructOnScope Args(F);C->ProcessEvent(F,Args.GetStructMemory());
   for(TFieldIterator<FDoubleProperty> P(F);P;++P)if(P->HasAnyPropertyFlags(CPF_OutParm|CPF_ReturnParm))return FText::AsNumber(P->GetPropertyValue_InContainer(Args.GetStructMemory()));
  }
 return FText::GetEmpty();
}
bool UPOICorridorTools::PatchWeaponHUD()
{
#if WITH_EDITOR
 auto* BP=LoadObject<UBlueprint>(nullptr,TEXT("/AST_InventorySystem/UI/WeaponHUD.WeaponHUD"));if(!BP)return false;
 TArray<UEdGraph*> Graphs;BP->GetAllGraphs(Graphs);
 for(auto* G:Graphs)if(G->GetName()==TEXT("GetText"))
 {
  BP->Modify();UEdGraphNode *Entry=nullptr,*Result=nullptr;
  for(UEdGraphNode* N:G->Nodes){if(N->IsA<UK2Node_FunctionEntry>())Entry=N;if(N->IsA<UK2Node_FunctionResult>())Result=N;}
  if(!Entry || !Result)return false;
  const auto Old=G->Nodes;for(UEdGraphNode* N:Old){N->BreakAllNodeLinks();if(N!=Entry && N!=Result)G->RemoveNode(N);}
  auto* Call=NewObject<UK2Node_CallFunction>(G);G->AddNode(Call);Call->CreateNewGuid();Call->SetFromFunction(StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UPOICorridorTools,SafeHUDHealth)));Call->AllocateDefaultPins();
  auto* Self=NewObject<UK2Node_Self>(G);G->AddNode(Self);Self->CreateNewGuid();Self->AllocateDefaultPins();
  const auto* Schema=GetDefault<UEdGraphSchema_K2>();
  if(!Schema->TryCreateConnection(Entry->FindPinChecked(TEXT("then")),Result->FindPinChecked(TEXT("execute"))) || !Schema->TryCreateConnection(Self->FindPinChecked(TEXT("self")),Call->FindPinChecked(TEXT("Widget"))) || !Schema->TryCreateConnection(Call->GetReturnValuePin(),Result->FindPinChecked(TEXT("ReturnValue"))))return false;
  FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP);return BP->Status!=BS_Error;
 }
#endif
 return false;
}
