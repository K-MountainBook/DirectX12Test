#include "Scene.h"
#include "Engine.h"
#include "App.h"
#include <d3d12.h>
#include "SharedStruct.h"
#include "VertexBuffer.h"
#include "ConstantBuffer.h"
#include "RootSignature.h"
#include "PipelineState.h"
#include <iostream>

Scene* g_Scene;

using namespace DirectX;

VertexBuffer* vertexBuffer;
ConstantBuffer* constantBuffer[Engine::FRAME_BUFFER_COUNT];
RootSignature* rootSignature;
PipelineState* pipelinestate;

bool Scene::init()
{
	Vertex verticles[3] = {};

	verticles[0].Position = XMFLOAT3(-1.0f, -1.0f, 0.0f);
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

	pipelinestate = new PipelineState();
	pipelinestate->SetInputLayout(Vertex::InputLayout);
	pipelinestate->SetRootSignature(rootSignature->Get());
	pipelinestate->SetVS(L"./x64/Debug/SampleVS.cso");
	pipelinestate->SetPS(L"./x64/Debug/SamplePS.cso");
	pipelinestate->Create();

	if (!pipelinestate->IsValid()) {
		printf("パイプラインステートの生成に失敗\n");
		return false;
	}

	printf("シーンの初期化に成功\n");
	return true;
}

float rotateY = 0.0f;

void Scene::Update()
{

	rotateY += 0.02f;
	auto currentIndex = g_Engine->CurrentBackBufferIndex(); // 現在のフレーム番号を取得
	auto currentTransform = constantBuffer[currentIndex]->GetPtr<Transform>(); // 現在のフレーム番号に対応する定数バッファを取得
	currentTransform->World = DirectX::XMMatrixRotationY(rotateY); // Y軸で回転させる
}

void Scene::Draw()
{
	auto currentIndex = g_Engine->CurrentBackBufferIndex();
	auto commandList = g_Engine->CommandList();
	auto vbView = vertexBuffer->View();

	commandList->SetGraphicsRootSignature(rootSignature->Get());
	commandList->SetPipelineState(pipelinestate->Get());
	commandList->SetGraphicsRootConstantBufferView(0, constantBuffer[currentIndex]->GetAddress());

	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->IASetVertexBuffers(0, 1, &vbView);

	commandList->DrawInstanced(3, 1, 0, 0);

}
