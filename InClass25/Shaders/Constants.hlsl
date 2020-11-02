// We want to use row major matrices
#pragma pack_matrix(row_major)

cbuffer PerCameraConstants : register(b0)
{
    float4x4 c_viewProj;
    float3 c_cameraPosition;
};

cbuffer PerObjectConstants : register(b1)
{
    float4x4 c_modelToWorld;
};

cbuffer LightConstants : register(b2)
{
    float3 c_lightDir;
    float3 c_lightColor;
};

SamplerState DefaultSampler : register(s0);
Texture2D DiffuseTexture : register(t0);
