#include "Scene.h"
#include "Engine.h"
#include "App.h"
#include <d3d12.h>
#include "SharedStruct.h"
#include "VertexBuffer.h"
#include "ConstantBuffer.h"
#include "RootSignature.h"

Scene* g_Scene;

using namespace DirectX;

VertexBuffer* vertexBuffer;
ConstantBuffer* constantBuffer[Engine::FRAME_BUFFER_COUNT];
RootSignature* rootSignature;

bool Scene::init()
{
	Vertex verticles[3] = {};

	verticles[0].Position = XMFLOAT3(-1.0f, -1.0f, -1.0f);
	verticles[0].Color = XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f);

	verticles[1].Position = XMFLOAT3(1.0f, -1.0f, 0.0f);
	verticles[1].Color = XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f);

	verticles[2].Position = XMFLOAT3(0.0f, 1.0f, 0.0f);
	verticles[2].Color = XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f);


	auto vertexSize = sizeof(Vertex) * std::size(verticles);
	auto vertexStride = sizeof(Vertex);
	vertexBuffer = new VertexBuffer(vertexSize, vertexStride, verticles);

	if (!vertexBuffer->IsValid()) {
		printf("頂点バッファの生成に失敗\n");
		return false;
	}

	auto eyePos = XMVectorSet(0.0f, 0.0f, 5.0f, 0.0f);
	auto targetPos = XMVectorZero();
	auto upward = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	auto fov = XMConvertToRadians(37.5);
	auto aspect = static_cast<float>(WINDOW_WIDTH) / static_cast<float>(WINDOW_HEIGHT);

	for (size_t i = 0; i < Engine::FRAME_BUFFER_COUNT; i++) {
		constantBuffer[i] = new ConstantBuffer(sizeof(Transform));
		if (!constantBuffer[i]->IsValid()) {
			printf("変換行列用定数バッファの生成に失敗\n");
			return false;
		}

		auto ptr = constantBuffer[i]->GetPtr<Transform>();
		ptr->World = XMMatrixIdentity();
		ptr->View = XMMatrixLookAtRH(eyePos, targetPos, upward);
		ptr->Proj = XMMatrixPerspectiveFovRH(fov, aspect, 0.3f, 1000.0f);

	}

	
	rootSignature = new RootSignature();
	if (!rootSignature->IsValid()) {
		printf("ルートシグネチャの生成に失敗\n");
		return false;
	}


	printf("シーンの初期化に成功\n");
	return true;
}

void Scene::Update()
{
}

void Scene::Draw()
{
}
