#pragma once
#include "assetManager.h"
#include "engineMath.h"
#include "Graphics.h"
#include <vector>

class Camera;
class RenderObj;
class Shader;
class Texture;

class Game
{
public:
	struct LightingConstants
	{
		Vector3 c_lightDir;
		float pad0;
		Vector3 c_lightColor;
		float pad1;
	};

    Game();
    ~Game();

    void Init(HWND hWnd, float width, float height);
    void Shutdown();
	void Update(float deltaTime);
    void RenderFrame();

	void OnKeyDown(uint32_t key);
	void OnKeyUp(uint32_t key);
	bool IsKeyHeld(uint32_t key) const;
	
	AssetManager* GetAssetManager() { return &mAssetManager; }

private:
	std::unordered_map<uint32_t, bool> m_keyIsHeld;

	Graphics mGraphics;
	AssetManager mAssetManager;
	
	std::vector<RenderObj*> mRenderObj;
	Camera* mCamera;

	LightingConstants mLightData;
	ID3D11Buffer* mLightConst;

	bool LoadLevel(const WCHAR* fileName);
};