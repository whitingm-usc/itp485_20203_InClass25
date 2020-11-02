#include "stdafx.h"
#include "Game.h"
#include "Camera.h"
#include "engineMath.h"
#include "Graphics.h"
#include "jsonUtil.h"
#include "RenderCube.h"
#include "RenderObj.h"
#include "Shader.h"
#include "SkinnedObj.h"
#include "stringUtil.h"
#include "texture.h"
#include "VertexBuffer.h"
#include "VertexFormat.h"
#include "rapidjson\include\rapidjson\rapidjson.h"
#include "rapidjson\include\rapidjson\document.h"
#include "Components\character.h"
#include <fstream>
#include <sstream>


Game::Game()
	: mCamera(nullptr)
	, mLightConst(nullptr)
	, mAssetManager(&mGraphics)
{
}

Game::~Game()
{
}

void Game::Init(HWND hWnd, float width, float height)
{
	mGraphics.InitD3D(hWnd, width, height);

	{	// load the Skinned shader
		D3D11_INPUT_ELEMENT_DESC inputElem[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,   0, offsetof(VertexSkinned, pos),		D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,   0, offsetof(VertexSkinned, norm),		D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "BONE",     0, DXGI_FORMAT_R8G8B8A8_UINT,		0, offsetof(VertexSkinned, boneIndex),	D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "WEIGHT",   0, DXGI_FORMAT_R8G8B8A8_UNORM,	0, offsetof(VertexSkinned, weight),		D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,		0, offsetof(VertexSkinned, uv),			D3D11_INPUT_PER_VERTEX_DATA, 0 }
		};
		Shader* pShader = new Shader(&mGraphics);
		bool ret = pShader->Load(L"Shaders/SkinnedToon.hlsl", inputElem, ARRAY_SIZE(inputElem));
		DbgAssert(ret, "Skinned Failed to Load");
		mAssetManager.SetShader(L"Skinned", pShader);
	}

	{	// create the camera
		mCamera = new Camera(&mGraphics);
	}

	{	// set up the lights
		mLightConst = mGraphics.CreateGraphicsBuffer(&mLightData, sizeof(mLightData), D3D11_BIND_CONSTANT_BUFFER, D3D11_CPU_ACCESS_WRITE, D3D11_USAGE_DYNAMIC);
	}

	LoadLevel(L"Assets/Levels/InClass25.itplevel");
}

void Game::Shutdown()
{
	mLightConst->Release();
	delete mCamera;
	for (RenderObj* pObj : mRenderObj)
		delete pObj;
	mRenderObj.clear();
	mAssetManager.Clear();
	mGraphics.CleanD3D();
}

void Game::Update(float deltaTime)
{
	for (RenderObj* pObj : mRenderObj)
		pObj->Update(deltaTime);
}

void Game::RenderFrame()
{
	// Clear the screen to blue
	Graphics::Color4 clearColor(0.0f, 0.2f, 0.4f, 1.0f);
	mGraphics.BeginFrame(clearColor);

	// Set the camera active
	mCamera->SetActive();

	{	// Upload the lights
		mGraphics.UploadBuffer(mLightConst, &mLightData, sizeof(mLightData));
		mGraphics.GetDeviceContext()->PSSetConstantBuffers(Graphics::CONSTANT_BUFFER_LIGHTING, 1, &mLightConst);
	}

	// Draw the objects
	for (RenderObj* pObj : mRenderObj)
	{
		pObj->Draw();
	}

	mGraphics.EndFrame();
}

void Game::OnKeyDown(uint32_t key)
{
	m_keyIsHeld[key] = true;
}

void Game::OnKeyUp(uint32_t key)
{
	m_keyIsHeld[key] = false;
}

bool Game::IsKeyHeld(uint32_t key) const
{
	const auto find = m_keyIsHeld.find(key);
	if (find != m_keyIsHeld.end())
		return find->second;
	return false;
}

bool Game::LoadLevel(const WCHAR* fileName)
{
	std::ifstream file(fileName);
	if (!file.is_open())
	{
		return false;
	}

	std::stringstream fileStream;
	fileStream << file.rdbuf();
	std::string contents = fileStream.str();
	rapidjson::StringStream jsonStr(contents.c_str());
	rapidjson::Document doc;
	doc.ParseStream(jsonStr);

	if (!doc.IsObject())
	{
		return false;
	}

	std::string str = doc["metadata"]["type"].GetString();
	int ver = doc["metadata"]["version"].GetInt();

	// Check the metadata
	if (!doc["metadata"].IsObject() ||
		str != "itplevel" ||
		ver != 2)
	{
		return false;
	}

	{	// Read the camera data
		const rapidjson::Value& camObj = doc["camera"];
		if (camObj.IsObject())
		{
			Vector3 pos = Vector3::Zero;
			GetVectorFromJSON(camObj, "position", pos);
			Quaternion rot = Quaternion::Identity;
			GetQuaternionFromJSON(camObj, "rotation", rot);
			mCamera->mWorldToCamera = Matrix4::CreateFromQuaternion(rot) * Matrix4::CreateTranslation(pos);
			mCamera->mWorldToCamera.Invert();
		}
	}

	{	// Read the lighting data
		const rapidjson::Value& lightObj = doc["lightingData"];
		if (lightObj.IsObject())
		{
			GetVectorFromJSON(lightObj, "lightDir", mLightData.c_lightDir);
			GetVectorFromJSON(lightObj, "lightColor", mLightData.c_lightColor);
		}
	}

	{   // Load the RenderObjs
		const rapidjson::Value& renderObjects = doc["renderObjects"];
		if (renderObjects.IsArray())
		{
			for (rapidjson::SizeType i = 0; i < renderObjects.Size(); ++i)
			{
				const rapidjson::Value& obj = renderObjects[i];
				Vector3 pos;
				GetVectorFromJSON(obj, "position", pos);
				Quaternion rot;
				GetQuaternionFromJSON(obj, "rotation", rot);
				float scale;
				GetFloatFromJSON(obj, "scale", scale);
				Mesh* pMesh = nullptr;
				std::wstring mesh;
				if (GetWStringFromJSON(obj, "mesh", mesh))
					pMesh = mAssetManager.LoadMesh(mesh);
				RenderObj* pObj = nullptr;
				SkinnedObj* pSkin = nullptr;
				if (nullptr != pMesh && pMesh->IsSkinned())
				{
					pSkin = new SkinnedObj(&mGraphics, pMesh);
					pObj = pSkin;
				}
				else
				{
					pObj = new RenderObj(&mGraphics, pMesh);
				}
				pObj->mPerObjectConst.c_modelToWorld =
					Matrix4::CreateScale(scale)
					* Matrix4::CreateFromQuaternion(rot)
					* Matrix4::CreateTranslation(pos);

				const rapidjson::Value& components = obj["components"];
				if (components.IsArray())
				{
					for (rapidjson::SizeType j = 0; j < components.Size(); ++j)
					{
						Component* pComp = nullptr;
						const rapidjson::Value& comp = components[j];
						std::string type;
						GetStringFromJSON(comp, "type", type);
						if (type == "Character")
						{
							pComp = new Character(this, pSkin);
						}
						if (nullptr != pComp)
						{
							pComp->LoadProperties(comp);
							pObj->AddComponent(pComp);
						}
					}
				}

				mRenderObj.push_back(pObj);
			}
		}
	}

	return true;
}