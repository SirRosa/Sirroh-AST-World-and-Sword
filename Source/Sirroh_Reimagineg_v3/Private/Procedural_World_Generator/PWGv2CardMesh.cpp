#include "Procedural_World_Generator/PWGv2Generator.h"
#include "Engine/StaticMesh.h"
#include "MeshDescription.h"
#include "MeshDescriptionBuilder.h"
#include "StaticMeshAttributes.h"

UStaticMesh* APWGv2Generator::GetOrCreateGrassCardMesh()
{
	if (CardMesh) return CardMesh;
	static TWeakObjectPtr<UStaticMesh> SharedGrassCard;
	if (SharedGrassCard.IsValid())
	{
		CardMesh = SharedGrassCard.Get();
		return CardMesh;
	}

	FMeshDescription Description;
	FStaticMeshAttributes Attributes(Description);
	Attributes.Register();
	FMeshDescriptionBuilder Builder;
	Builder.SetMeshDescription(&Description);
	Builder.SetNumUVLayers(1);
	const FPolygonGroupID Group = Builder.AppendPolygonGroup(TEXT("Grass"));
	constexpr int32 VerticalSegments = 4;
	FVertexID Vertices[VerticalSegments + 1][2];
	for (int32 Row = 0; Row <= VerticalSegments; ++Row)
	{
		const float Alpha = static_cast<float>(Row) / VerticalSegments;
		Vertices[Row][0] = Builder.AppendVertex(FVector(-50.0f, 0.0f, Alpha * 100.0f));
		Vertices[Row][1] = Builder.AppendVertex(FVector( 50.0f, 0.0f, Alpha * 100.0f));
	}
	auto MakeInstance = [&](const FVertexID Vertex, const FVector2D UV)
	{
		const FVertexInstanceID Instance = Builder.AppendInstance(Vertex);
		Builder.SetInstanceUV(Instance, UV);
		Builder.SetInstanceTangentSpace(Instance, -FVector::YAxisVector, FVector::XAxisVector, 1.0f);
		Builder.SetInstanceColor(Instance, FVector4f(1.0f));
		return Instance;
	};
	for (int32 Row = 0; Row < VerticalSegments; ++Row)
	{
		const float V0 = 1.0f - static_cast<float>(Row) / VerticalSegments;
		const float V1 = 1.0f - static_cast<float>(Row + 1) / VerticalSegments;
		const FVertexInstanceID LB = MakeInstance(Vertices[Row][0], FVector2D(0.0f, V0));
		const FVertexInstanceID RB = MakeInstance(Vertices[Row][1], FVector2D(1.0f, V0));
		const FVertexInstanceID RT = MakeInstance(Vertices[Row + 1][1], FVector2D(1.0f, V1));
		Builder.AppendTriangle(LB, RB, RT, Group);
		const FVertexInstanceID LB2 = MakeInstance(Vertices[Row][0], FVector2D(0.0f, V0));
		const FVertexInstanceID RT2 = MakeInstance(Vertices[Row + 1][1], FVector2D(1.0f, V1));
		const FVertexInstanceID LT = MakeInstance(Vertices[Row + 1][0], FVector2D(0.0f, V1));
		Builder.AppendTriangle(LB2, RT2, LT, Group);
	}

	UStaticMesh* Mesh = NewObject<UStaticMesh>(GetTransientPackage(), TEXT("PWGv2_RuntimeGrassCard"), RF_Transient);
	Mesh->GetStaticMaterials().Add(FStaticMaterial(nullptr, TEXT("Grass")));
	UStaticMesh::FBuildMeshDescriptionsParams Params;
	Params.bMarkPackageDirty = false;
	Params.bBuildSimpleCollision = false;
	Params.bFastBuild = true;
	Params.bAllowCpuAccess = false;
	TArray<const FMeshDescription*> Descriptions;
	Descriptions.Add(&Description);
	if (!Mesh->BuildFromMeshDescriptions(Descriptions, Params)) return nullptr;
	SharedGrassCard = Mesh;
	CardMesh = Mesh;
	return CardMesh;
}
