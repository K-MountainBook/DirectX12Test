#pragma once
class Scene
{
public:
	bool init();

	void Update();
	void Draw();
};

extern Scene* g_Scene;

