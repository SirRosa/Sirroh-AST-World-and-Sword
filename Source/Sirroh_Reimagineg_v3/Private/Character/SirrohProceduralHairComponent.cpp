#include "Character/SirrohProceduralHairComponent.h"
#include "Character/SirrohPoseBridgeComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/SkeletalBodySetup.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"
#include "SirrohHairScalpData.inl"
namespace {
struct FScalpInfluence { const TCHAR* Bone; float Weight; };
#include "SirrohHairScalpWeights.inl"
static_assert(UE_ARRAY_COUNT(SimpleManScalp)==UE_ARRAY_COUNT(SimpleManScalpWeights));
static_assert(UE_ARRAY_COUNT(GrimGirlScalp)==UE_ARRAY_COUNT(GrimGirlScalpWeights));
constexpr int32 Steps=12;
FLinearColor HairColor(uint8 I) {
    static const FColor Palette[]={FColor(24,22,24),FColor(75,48,31),FColor(139,122,88),FColor(206,201,184),FColor(116,47,26)};
    return FLinearColor::FromSRGBColor(Palette[FMath::Min<int32>(I,4)]);
}
}
USirrohProceduralHairComponent::USirrohProceduralHairComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {
    PrimaryComponentTick.bCanEverTick=true;PrimaryComponentTick.TickGroup=TG_PostPhysics;
    bTickInEditor=true;
    SetCollisionEnabled(ECollisionEnabled::NoCollision);SetCastShadow(false);
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> M(TEXT("/Game/Characters/ProceduralHair/M_ProceduralHair.M_ProceduralHair"));
    HairMaterial=M.Object;bUseAsyncCooking=true;
}
void USirrohProceduralHairComponent::RefreshVisiblePose()
{
    const auto& Skeleton=Source->GetSkeletalMeshAsset()->GetRefSkeleton();
    const auto* Bridge=GetOwner()->FindComponentByClass<USirrohPoseBridgeComponent>();
    const auto* Visual=Bridge && Bridge->bCreateRuntime3DVisual?Bridge->GetVisualMesh():nullptr;
    VisiblePose.SetNum(Skeleton.GetNum());
    if(Visual && Visual->GetSkinnedAsset()==Source->GetSkeletalMeshAsset() && Visual->BoneSpaceTransforms.Num()==VisiblePose.Num()){
        // Use committed locals, not cached component transforms waiting for the renderer to refresh.
        for(int32 I=0;I<VisiblePose.Num();++I){
            const int32 Parent=Skeleton.GetParentIndex(I);
            VisiblePose[I]=Visual->BoneSpaceTransforms[I]*(Parent==INDEX_NONE?Visual->GetComponentTransform():VisiblePose[Parent]);
        }
    }else{
        // GetBoneTransform resolves LeaderPose mappings as well as ordinary animated meshes.
        for(int32 I=0;I<VisiblePose.Num();++I)VisiblePose[I]=Source->GetBoneTransform(I);
    }
}
FVector USirrohProceduralHairComponent::SkinScalpPoint(int32 Index,const FVector& Point) const
{
    const auto* Weights=Settings.Sex==ESirrohSex::Female?GrimGirlScalpWeights[Index]:SimpleManScalpWeights[Index];
    FVector Result=FVector::ZeroVector;float Total=0;
    const auto& Skeleton=Source->GetSkeletalMeshAsset()->GetRefSkeleton();
    for(int32 K=0;K<4;++K)if(Weights[K].Weight>0){
        const int32 Bone=Skeleton.FindBoneIndex(Weights[K].Bone);
        if(VisiblePose.IsValidIndex(Bone)){Result+=VisiblePose[Bone].TransformPosition(InverseReferencePose[Bone].TransformPosition(Point))*Weights[K].Weight;Total+=Weights[K].Weight;}
    }
    return Total>SMALL_NUMBER?Result/Total:Target(Point,CurrentHead());
}
FTransform USirrohProceduralHairComponent::CurrentHead() const {
    return VisiblePose.IsValidIndex(HeadBone)?VisiblePose[HeadBone]:Source->GetSocketTransform(TEXT("head"),RTS_World);
}
FVector USirrohProceduralHairComponent::Target(const FVector& P,const FTransform& Head) const {return Head.TransformPosition(RefHead.InverseTransformPosition(P));}
void USirrohProceduralHairComponent::Configure(USkeletalMeshComponent* InMesh,const FSirrohCharacterProfile& P) {
    if(!IsValid(InMesh) || !InMesh->GetSkeletalMeshAsset()){ClearAllMeshSections();bReady=false;SetComponentTickEnabled(false);return;}
    const bool Supported=P.Sex==ESirrohSex::Female || P.BodyType==ESirrohBodyType::SimpleMan;
    SetVisibility(P.bHairEnabled && Supported);SetHiddenInGame(!(P.bHairEnabled && Supported));
    if(!P.bHairEnabled || !Supported){SetComponentTickEnabled(false);bReady=false;ClearAllMeshSections();return;}
    const bool Same=bReady && Source==InMesh && Settings.Sex==P.Sex && Settings.HairSeed==P.HairSeed && FMath::IsNearlyEqual(Settings.HairLength,P.HairLength) && Settings.HairFrontLength==P.HairFrontLength && Settings.HairSideLength==P.HairSideLength && Settings.HairBackLength==P.HairBackLength && Settings.HairTopLength==P.HairTopLength && Settings.HairStyle==P.HairStyle && Settings.HairDensity==P.HairDensity && Settings.HairColor==P.HairColor && Settings.HairHighlightColor==P.HairHighlightColor;
    if(Same)return;
    Source=InMesh;Settings=P;
    const auto& S=Source->GetSkeletalMeshAsset()->GetRefSkeleton();int32 I=S.FindBoneIndex(TEXT("head"));if(I==INDEX_NONE)return;
    HeadBone=I;
    TArray<FTransform> Reference=S.GetRefBonePose();InverseReferencePose.SetNum(Reference.Num());
    for(int32 B=0;B<Reference.Num();++B){const int32 Parent=S.GetParentIndex(B);if(Parent!=INDEX_NONE)Reference[B]=Reference[B]*Reference[Parent];InverseReferencePose[B]=Reference[B].Inverse();}
    RefHead=Reference[HeadBone];RefreshVisiblePose();
    AddTickPrerequisiteComponent(Source);
    if(auto* B=GetOwner()->FindComponentByClass<USirrohPoseBridgeComponent>())AddTickPrerequisiteComponent(B);
    SetMaterial(0,HairMaterial);SetComponentTickEnabled(true);Rebuild();
}
// Radial collision sampled from the actual authored Cranium, cached per head type.
// The lower, uncovered face/neck retains an ellipsoid fallback.
void USirrohProceduralHairComponent::BuildScalpCollision()
{
    constexpr int32 Az=48, El=32;
    const FVector Center(0,-1,163), Radius(8,9,10.8);
    ScalpCollisionRadii.SetNum(Az*El);CollisionSex=Settings.Sex;MaxCollisionRadius=0.f;
    const FVector* Envelope=Settings.Sex==ESirrohSex::Female?GrimGirlHeadEnvelope:SimpleManHeadEnvelope;
    const int32* Indices=Settings.Sex==ESirrohSex::Female?GrimGirlHeadEnvelopeIndices:SimpleManHeadEnvelopeIndices;
    const int32 Count=Settings.Sex==ESirrohSex::Female?UE_ARRAY_COUNT(GrimGirlHeadEnvelopeIndices):UE_ARRAY_COUNT(SimpleManHeadEnvelopeIndices);
    for(int32 Y=0;Y<El;++Y)for(int32 X=0;X<Az;++X){
        const float Theta=PI*Y/(El-1),Phi=2*PI*X/Az;
        FVector D(FMath::Sin(Theta)*FMath::Cos(Phi),FMath::Sin(Theta)*FMath::Sin(Phi),FMath::Cos(Theta));
        float Distance=0;
        for(int32 T=0;T<Count;T+=3){
            const FVector A=Envelope[Indices[T]],B=Envelope[Indices[T+1]],C=Envelope[Indices[T+2]];
            const FVector E1=B-A,E2=C-A,P=FVector::CrossProduct(D,E2);
            const double Det=FVector::DotProduct(E1,P);if(FMath::Abs(Det)<1.e-8)continue;
            const FVector S=Center-A;const double U=FVector::DotProduct(S,P)/Det;if(U<0 || U>1)continue;
            const FVector Q=FVector::CrossProduct(S,E1);const double V=FVector::DotProduct(D,Q)/Det;if(V<0 || U+V>1)continue;
            const double Hit=FVector::DotProduct(E2,Q)/Det;if(Hit>0)Distance=FMath::Max(Distance,float(Hit));
        }
        ScalpCollisionRadii[Y*Az+X]=Distance>0?Distance:1.f/(D/Radius).Size();
        MaxCollisionRadius=FMath::Max(MaxCollisionRadius,ScalpCollisionRadii[Y*Az+X]);
    }
}
FVector USirrohProceduralHairComponent::ProjectOutsideScalp(const FVector& Point,const FTransform& Head,float Margin) const
{
    constexpr int32 Az=48,El=32;const FVector Center(0,-1,163);
    FVector Ref=RefHead.TransformPosition(Head.InverseTransformPosition(Point));const FVector Delta=Ref-Center;
    const float Distance=Delta.Size();if(Distance<SMALL_NUMBER || Distance>=MaxCollisionRadius+Margin)return Point;
    const FVector D=Delta/Distance;
    float X=FMath::Atan2(D.Y,D.X)*Az/(2*PI);if(X<0)X+=Az;
    const float Y=FMath::Acos(FMath::Clamp(D.Z,-1.0,1.0))*(El-1)/PI;
    const int32 X0=FMath::FloorToInt(X)%Az,X1=(X0+1)%Az,Y0=FMath::Min(FMath::FloorToInt(Y),El-1),Y1=FMath::Min(Y0+1,El-1);
    const float R=FMath::Lerp(FMath::Lerp(ScalpCollisionRadii[Y0*Az+X0],ScalpCollisionRadii[Y0*Az+X1],X-FMath::FloorToFloat(X)),FMath::Lerp(ScalpCollisionRadii[Y1*Az+X0],ScalpCollisionRadii[Y1*Az+X1],X-FMath::FloorToFloat(X)),Y-Y0)+Margin;
    return Distance<R?Target(Center+D*R,Head):Point;
}
void USirrohProceduralHairComponent::Rebuild() {
    Scalp.Reset();ScalpIndices.Reset();Locks.Reset();
    if(Settings.Sex==ESirrohSex::Female){Scalp.Append(GrimGirlScalp,UE_ARRAY_COUNT(GrimGirlScalp));ScalpIndices.Append(GrimGirlScalpIndices,UE_ARRAY_COUNT(GrimGirlScalpIndices));}
    else{Scalp.Append(SimpleManScalp,UE_ARRAY_COUNT(SimpleManScalp));ScalpIndices.Append(SimpleManScalpIndices,UE_ARRAY_COUNT(SimpleManScalpIndices));}
    if(ScalpCollisionRadii.IsEmpty() || CollisionSex!=Settings.Sex)BuildScalpCollision();
    FRandomStream R(Settings.HairSeed);TArray<float> Areas;float Total=0;
    for(int32 I=0;I<ScalpIndices.Num();I+=3){Total+=FVector::CrossProduct(Scalp[ScalpIndices[I+1]]-Scalp[ScalpIndices[I]],Scalp[ScalpIndices[I+2]]-Scalp[ScalpIndices[I]]).Size();Areas.Add(Total);}
    LastHead=CurrentHead();
    const int32 LockCount=FMath::RoundToInt(160*Settings.HairDensity);
    for(int32 I=0;I<LockCount;++I){
        const float Pick=(I+R.FRand())/float(LockCount)*Total;int32 T=0;while(T<Areas.Num()-1 && Areas[T]<Pick)++T;
        float U=FMath::Sqrt(R.FRand()),V=R.FRand();FVector Root=(1-U)*Scalp[ScalpIndices[T*3]]+U*(1-V)*Scalp[ScalpIndices[T*3+1]]+U*V*Scalp[ScalpIndices[T*3+2]];
        const FVector Center(0,-1,163), Radius(8.0,9.0,10.8);
        FVector N=(Root-Center).GetSafeNormal();Root+=N*.35f;
        FLock L;L.RootTriangle=T*3;L.Bary=FVector(1-U,U*(1-V),U*V);L.Color=HairColor(I%5==0?Settings.HairHighlightColor:Settings.HairColor);
        const FVector E=((Root-Center)/Radius).GetSafeNormal();
        const float Theta0=FMath::Acos(FMath::Clamp(E.Z,-1.0,1.0));
        const float Phi0=FMath::Atan2(E.Y,E.X);
        const float PhiTarget=Root.Y>1 ? (Root.X>=0?0.f:PI) : Phi0;
        const float PhiDelta=FMath::FindDeltaAngleRadians(Phi0,PhiTarget);
        L.Width=FVector(-FMath::Sin(Phi0),FMath::Cos(Phi0),0);
        const FVector RootOffset=Root-(Center+E*Radius);
        // Smooth regional weights avoid a hard seam between crown, fringe, temples and nape.
        const float Top=FMath::SmoothStep(.55f,.9f,float(E.Z));
        const float Front=(1-Top)*FMath::SmoothStep(.2f,.75f,float(E.Y));
        const float Back=(1-Top)*FMath::SmoothStep(.1f,.7f,float(-E.Y));
        const float Side=FMath::Max(0.f,1-Top-Front-Back);
        auto Regional=[this](float Value){return Value>0?Value:Settings.HairLength;};
        const float Length=(Top*Regional(Settings.HairTopLength)+Front*Regional(Settings.HairFrontLength)+Back*Regional(Settings.HairBackLength)+Side*Regional(Settings.HairSideLength))*R.FRandRange(.94f,1.04f);
        L.CurlPhase=R.FRandRange(0.f,2*PI);
        L.CurlRadius=Settings.HairStyle==ESirrohHairStyle::Curly?.65f:(Settings.HairStyle==ESirrohHairStyle::Wavy?.85f:0.f);
        L.CurlFrequency=Settings.HairStyle==ESirrohHairStyle::Curly?2.5f:1.f;
        L.Pinned=1; // Only the root tangent is fixed; the skull envelope supports the remaining chain.
        if(Settings.HairStyle==ESirrohHairStyle::Spiky)L.Pinned=FMath::Max(1,Steps/3);
        L.CurlRadius=FMath::Min(L.CurlRadius,Length*.12f);
        // Keep the skull-following section kinematic; simulate only the hanging tips.
        for(int32 J=0;J<=Steps;++J){
            const float Travel=Length*float(J)/Steps;
            const float Arc=FMath::Min(Travel,FMath::Max(0.f,(1.8f-Theta0)*9.f));
            const float Theta=Theta0+Arc/9.f;
            const float Phi=Phi0+PhiDelta*(1-Front)*FMath::SmoothStep(0.f,10.f,Travel);
            FVector Q=Center+FVector(FMath::Sin(Theta)*FMath::Cos(Phi),FMath::Sin(Theta)*FMath::Sin(Phi),FMath::Cos(Theta))*Radius;
            Q+=RootOffset*FMath::Exp(-Travel/4.f);
            Q.Z-=Travel-Arc;
            if(Settings.HairStyle==ESirrohHairStyle::Spiky) Q=Root+N*Travel;
            L.Rest.Add(Q);L.Current.Add(Target(Q,LastHead));L.Previous.Add(L.Current.Last());
        }
        L.GroomTargets.SetNum(Steps+1);
        L.SegmentLengths.SetNum(Steps+1);L.FreeRadii.SetNum(Steps+1);
        for(int32 J=0;J<=Steps;++J){
            L.SegmentLengths[J]=J>0?(L.Rest[J]-L.Rest[J-1]).Size():0.f;
            L.FreeRadii[J]=FMath::Min(14.f,2.f+float((L.Rest[J]-L.Rest[L.Pinned]).Size())*.55f);
        }
        Locks.Add(MoveTemp(L));
    }
    RenderStride=1;Accumulator=0;bReady=true;UpdateGeometry(LastHead,true);
}
void USirrohProceduralHairComponent::UpdateGeometry(const FTransform& Head,bool Create) {
    Vertices.Reset();Normals.Reset();Colors.Reset();UV.Reset();Tangents.Reset();if(Create)Triangles.Reset();
    const FTransform Inv=GetComponentTransform().Inverse();
    for(int32 ScalpIndex=0;ScalpIndex<Scalp.Num();++ScalpIndex){
        const FVector& P=Scalp[ScalpIndex];
        const FVector Radial=(P-FVector(0,-1,163)).GetSafeNormal();
        Vertices.Add(Inv.TransformPosition(SkinScalpPoint(ScalpIndex,P+Radial*.45f)));
        Normals.Add(Inv.TransformVectorNoScale(Head.TransformVectorNoScale(RefHead.InverseTransformVectorNoScale(Radial))));
        Colors.Add(HairColor(Settings.HairColor));
        UV.Add(FVector2D((FMath::Atan2(Radial.Y,Radial.X)/float(2*PI)+.5f)*28.f,.5f));Tangents.Add(FProcMeshTangent(1,0,0));
    }
    if(Create)Triangles.Append(ScalpIndices);
    // Two narrow crossed ribbons keep locks visible from the front and side.
    constexpr int32 RenderSteps=Steps*2;
    for(int32 LockIndex=0;LockIndex<Locks.Num();LockIndex+=RenderStride)for(int32 Plane=0;Plane<2;++Plane){
        const FLock& L=Locks[LockIndex];
        int32 Base=Vertices.Num();
        for(int32 J=0;J<=RenderSteps;++J){
            const float Sample=float(J)*Steps/RenderSteps;const int32 Joint=FMath::Min(FMath::FloorToInt(Sample),Steps);
            const int32 Next=FMath::Min(Joint+1,Steps);
            const FVector Position=FMath::Lerp(L.Current[Joint],L.Current[Next],Sample-Joint);
            FVector RD=L.Rest[FMath::Min(Joint+1,Steps)]-L.Rest[FMath::Max(Joint-1,0)];
            FVector RW=FVector::CrossProduct(RD,(L.Rest[Joint]-FVector(0,-1,163)).GetSafeNormal()).GetSafeNormal();
            if(RW.IsNearlyZero())RW=L.Width;

            FVector W=Head.TransformVectorNoScale(RefHead.InverseTransformVectorNoScale(RW)).GetSafeNormal();
            float A=float(J)/RenderSteps;float Width=((Plane?.5f:.85f)*FMath::Pow(1-A,.6f)+.025f)*FMath::Sqrt(float(RenderStride));
            FVector D=L.Current[FMath::Min(Joint+1,Steps)]-L.Current[FMath::Max(Joint-1,0)];
            D.Normalize();W=(W-D*FVector::DotProduct(W,D)).GetSafeNormal();
            if(W.IsNearlyZero())W=FVector::CrossProduct(D,FVector::ForwardVector).GetSafeNormal();
            const FVector Binormal=FVector::CrossProduct(D,W).GetSafeNormal();
            const float Phase=A*2*PI*L.CurlFrequency+L.CurlPhase;
            const float Envelope=FMath::SmoothStep(0.f,.2f,A)*L.CurlRadius;
            const FVector Curl=Envelope*(W*FMath::Sin(Phase)+(Settings.HairStyle==ESirrohHairStyle::Curly?Binormal*FMath::Cos(Phase):FVector::ZeroVector));
            const FVector RenderPoint=J==0?Position:ProjectOutsideScalp(Position+Curl,Head,.7f);
            if(Plane)W=W.RotateAngleAxis(65.f,D);
            FVector N=Inv.TransformVectorNoScale(FVector::CrossProduct(W,D).GetSafeNormal());
            for(int32 K=0;K<2;++K){Vertices.Add(Inv.TransformPosition(ProjectOutsideScalp(RenderPoint+W*Width*(K?1:-1),Head,.7f)));Normals.Add(N);Colors.Add(L.Color);UV.Add(FVector2D(K,A));Tangents.Add(FProcMeshTangent(Inv.TransformVectorNoScale(W),false));}
            if(Create && J<RenderSteps){int32 B=Base+J*2;Triangles.Append({B,B+2,B+1,B+1,B+2,B+3});}
        }
    }
    if(Create)CreateMeshSection_LinearColor(0,Vertices,Triangles,Normals,UV,Colors,Tangents,false);
    else UpdateMeshSection_LinearColor(0,Vertices,Normals,UV,Colors,Tangents);
}
void USirrohProceduralHairComponent::TickComponent(float Dt,ELevelTick Type,FActorComponentTickFunction* Fn) {
    Super::TickComponent(Dt,Type,Fn);if(!bReady || !IsValid(Source))return;
    if(GetOwner()->IsHidden() || !IsVisible()) { Accumulator=0.f;return; }
    RefreshVisiblePose();
    const FTransform Head=CurrentHead();const bool Reset=(Head.GetLocation()-LastHead.GetLocation()).SizeSquared()>10000 || Dt>.2f;
    if(Reset){for(FLock& L:Locks)for(int32 I=0;I<L.Current.Num();++I)L.Previous[I]=L.Current[I]=Target(L.Rest[I],Head);Accumulator=0;}
    int32 NextStride=RenderStride;
    if(GetWorld()->IsGameWorld() && Cast<APawn>(GetOwner()))if(auto* PC=GetWorld()->GetFirstPlayerController())if(PC->PlayerCameraManager){
        const float Distance=FVector::Distance(PC->PlayerCameraManager->GetCameraLocation(),Head.GetLocation());
        if(RenderStride==1 && Distance>1400)NextStride=2;
        else if(RenderStride==2){if(Distance<1100)NextStride=1;else if(Distance>2800)NextStride=4;}
        else if(RenderStride==4 && Distance<2400)NextStride=2;
    }
    const bool ChangedLOD=NextStride!=RenderStride;
    if(NextStride<RenderStride)for(int32 K=0;K<Locks.Num();K+=NextStride)if(K%RenderStride!=0)
        for(int32 J=0;J<=Steps;++J)Locks[K].Previous[J]=Locks[K].Current[J]=Target(Locks[K].Rest[J],Head);
    const int32 PreviousStride=RenderStride;
    RenderStride=NextStride;
    struct FCapsule { FVector A,B; float Radius; FVector Axis; double InverseLengthSquared=0,RadiusSquared=0; };
    TArray<FCapsule,TInlineAllocator<32>> BodyColliders;
    struct FBoxCollider { FTransform World; FVector Extent; FBox Bounds; };
    TArray<FBoxCollider,TInlineAllocator<16>> BodyBoxes;
    // Reuse the original PA's torso/shoulder collision shapes, without changing the asset.
    auto* Bridge=GetOwner()->FindComponentByClass<USirrohPoseBridgeComponent>();
    if(auto* PA=Source->GetPhysicsAsset())for(const auto& Body:PA->SkeletalBodySetups){
        if(!Body)continue;const FString Bone=Body->BoneName.ToString();
        if(!(Bone.StartsWith(TEXT("spine")) || Bone.StartsWith(TEXT("neck")) || Bone.StartsWith(TEXT("clavicle")) || Bone.StartsWith(TEXT("upperarm"))))continue;
        const int32 BoneIndex=Source->GetBoneIndex(Body->BoneName);
        const FTransform BoneWorld=VisiblePose.IsValidIndex(BoneIndex)?VisiblePose[BoneIndex]:Source->GetSocketTransform(Body->BoneName);
        const float Scale=BoneWorld.GetScale3D().GetAbsMax();
        for(const auto& Box:Body->AggGeom.BoxElems){
            const FTransform World=Box.GetTransform()*BoneWorld;
            const FVector Extent=FVector(Box.X,Box.Y,Box.Z)*.5f+FVector(.5f);
            BodyBoxes.Add({World,Extent,FBox(-Extent,Extent).TransformBy(World)});
        }
        for(const auto& Sphere:Body->AggGeom.SphereElems){const FVector C=BoneWorld.TransformPosition(Sphere.Center);BodyColliders.Add({C,C,Sphere.Radius*Scale+.5f});}
        for(const auto& Capsule:Body->AggGeom.SphylElems){const FTransform W=Capsule.GetTransform()*BoneWorld;const FVector Axis=W.TransformVector(FVector(0,0,Capsule.Length*.5f));BodyColliders.Add({W.GetLocation()-Axis,W.GetLocation()+Axis,Capsule.Radius*Scale+.5f});}
    }
    for(auto& Capsule:BodyColliders){
        Capsule.Axis=Capsule.B-Capsule.A;
        const double LengthSquared=Capsule.Axis.SizeSquared();
        Capsule.InverseLengthSquared=LengthSquared>SMALL_NUMBER?1.0/LengthSquared:0.0;
        Capsule.RadiusSquared=FMath::Square(Capsule.Radius);
    }
    // Carry most frame motion with the head; leave a small, bounded inertial lag.
    // Transform both history samples identically so frame transport cannot create fake velocity.
    for(int32 K=0;K<Locks.Num();K+=RenderStride){auto& L=Locks[K];
        L.RootShift=FVector::ZeroVector;
        for(int32 Corner=0;Corner<3;++Corner)L.RootShift+=SkinScalpPoint(ScalpIndices[L.RootTriangle+Corner],L.Rest[0])*L.Bary[Corner];
        L.RootShift-=Target(L.Rest[0],Head);
        // The groom and rest lengths are invariant across all solver substeps this frame.
        for(int32 J=0;J<=Steps;++J)L.GroomTargets[J]=Target(L.Rest[J],Head)+L.RootShift;
        for(int32 J=0;J<=Steps;++J){
            if(J<=L.Pinned){L.Current[J]=L.Previous[J]=L.GroomTargets[J];continue;}
            if(Reset || (NextStride<PreviousStride && K%PreviousStride!=0)){
                L.Current[J]=L.Previous[J]=L.GroomTargets[J];continue;
            }
            const FVector Carried=Head.TransformPosition(LastHead.InverseTransformPosition(L.Current[J]));
            const float FreeFraction=float(J-L.Pinned)/FMath::Max(1,Steps-L.Pinned);
            const FVector Shift=(Carried-L.Current[J])*FMath::Lerp(.7f,.35f,FreeFraction);
            L.Current[J]+=Shift;L.Previous[J]+=Shift;
        }
    }
    constexpr float H=1.f/120;Accumulator=FMath::Min(Accumulator+FMath::Clamp(Dt,0.f,.1f),8*H);
    while(Accumulator>=H){Accumulator-=H;
        for(int32 K=0;K<Locks.Num();K+=RenderStride){FLock& L=Locks[K];
            for(int32 J=L.Pinned+1;J<=Steps;++J){
                const FVector Old=L.Current[J];
                const FVector Velocity=((L.Current[J]-L.Previous[J])/H).GetClampedToMaxSize(180.f);
                L.Current[J]+=Velocity*FMath::Exp(-4.f*H)*H+FVector(0,0,-600)*H*H;
                // Mild spring around the groom, not a hard world-space pin.
                L.Current[J]+=(L.GroomTargets[J]-L.Current[J])*(Settings.HairStyle==ESirrohHairStyle::Spiky?90.f:12.f)*H*H;
                L.Previous[J]=Old;
            }
            for(int32 Pass=0;Pass<6;++Pass)for(int32 J=L.Pinned+1;J<=Steps;++J){
                const FVector Before=L.Current[J];
                FVector Delta=L.Current[J]-L.Current[J-1];float Len=Delta.Size();float Rest=L.SegmentLengths[J];
                if(Len>SMALL_NUMBER){FVector C=Delta*(1-Rest/Len);if(J==L.Pinned+1)L.Current[J]-=C;else{L.Current[J]-=C*.5;L.Current[J-1]+=C*.5;L.Previous[J-1]+=C*.5;}}
                const FVector Groom=L.GroomTargets[J];
                L.Current[J]=Groom+(L.Current[J]-Groom).GetClampedToMaxSize(L.FreeRadii[J]);
                for(const auto& Capsule:BodyColliders){
                    const FVector& Axis=Capsule.Axis;
                    const float T=FMath::Clamp(FVector::DotProduct(L.Current[J]-Capsule.A,Axis)*Capsule.InverseLengthSquared,0.0,1.0);
                    const FVector Center=Capsule.A+T*Axis,DeltaBody=L.Current[J]-Center;
                    const double DistanceSquared=DeltaBody.SizeSquared();
                    if(DistanceSquared<Capsule.RadiusSquared && DistanceSquared>FMath::Square(SMALL_NUMBER))
                        L.Current[J]=Center+DeltaBody*(Capsule.Radius/FMath::Sqrt(DistanceSquared));
                }
                for(const auto& Box:BodyBoxes){
                    if(!Box.Bounds.IsInsideOrOn(L.Current[J]))continue;
                    FVector Local=Box.World.InverseTransformPosition(L.Current[J]);const FVector Depth=Box.Extent-Local.GetAbs();
                    if(Depth.X>0 && Depth.Y>0 && Depth.Z>0){
                        const int32 Axis=Depth.X<Depth.Y?(Depth.X<Depth.Z?0:2):(Depth.Y<Depth.Z?1:2);
                        Local[Axis]=(Local[Axis]>=0?1:-1)*Box.Extent[Axis];L.Current[J]=Box.World.TransformPosition(Local);
                    }
                }
                L.Current[J]=ProjectOutsideScalp(L.Current[J],Head,.8f);
                const FVector Mid=(L.Current[J]+L.Current[J-1])*.5;
                L.Current[J]+=2*(ProjectOutsideScalp(Mid,Head,.65f)-Mid);
                // Constraint corrections are positional: do not turn them into next-step launch velocity.
                L.Previous[J]+=L.Current[J]-Before;
            }
        }
    }
    LastHead=Head;UpdateGeometry(Head,ChangedLOD);
}

USirrohProceduralHairComponent* USirrohProceduralHairComponent::CreateForMesh(USkeletalMeshComponent* InMesh,const FSirrohCharacterProfile& Profile) {
    if(!IsValid(InMesh) || !InMesh->GetOwner())return nullptr;
    AActor* Owner=InMesh->GetOwner();auto* Hair=Owner->FindComponentByClass<USirrohProceduralHairComponent>();
    if(!Hair){Hair=NewObject<USirrohProceduralHairComponent>(Owner,TEXT("ProceduralHair"));Owner->AddInstanceComponent(Hair);Hair->SetupAttachment(Owner->GetRootComponent());Hair->RegisterComponent();}
    Hair->Configure(InMesh,Profile);return Hair;
}
