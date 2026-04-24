#include "Scene.h"
#include "Engine.h"
#include "App.h"
#include <d3d12.h>
#include "SharedStruct.h"

Scene* g_Scene;

using namespace DirectX;

bool Scene::init()
{
	Vertex verticles[3] = {};

	verticles[0].Position = XMFLOAT3(-1.0f, -1.0f, -1.0f);
	verticles[0].Color = XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f);

	verticles[1].Position = XMFLOAT3(1.0f, -1.0f, 0.0f);
	verticles[1].Color = XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f);

	verticles[2].Position = XMFLOAT3(0.0f, 1.0f, 0.0f);
	verticles[2].Color = XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f);



	printf("ÉVÅ[ÉìÇÃèâä˙âªÇ…ê¨å˜\n");

	return true;
}

void Scene::Update()
{
}

void Scene::Draw()
{
}
