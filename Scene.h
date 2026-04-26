#pragma once
class Scene
{
public:
	bool Init();

	void Update();
	void Draw();
};

extern Scene* g_Scene;

