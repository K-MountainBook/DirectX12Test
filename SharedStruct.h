#pragma once
#include <d3dx12.h>
#include <DirectXMath.h>
#include "ComPtr.h"

struct Vertex {
	DirectX::XMFLOAT3	Position;
	DirectX::XMFLOAT3	Normal;
	DirectX::XMFLOAT2	UV;
	DirectX::XMFLOAT3	Tangent;
	DirectX::XMFLOAT4	Color;
};
