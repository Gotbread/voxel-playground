#pragma once

#include <Windows.h>
#include <memory>

#include "Graphics.h"
#include "Camera.h"
#include "Math3D.h"


class Application
{
public:
	bool Init(HINSTANCE hInstance);
	void Run();
private:
	struct Vertex
	{
		float x, y, z;
		DWORD color;
	};
	struct matrix_cbuffer
	{
		Math3D::Matrix4x4 world, view, proj;
	};

	LRESULT CALLBACK WndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
	static LRESULT CALLBACK sWndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);

	bool initGraphics();
	bool initShaders();
	bool initGeometry();
	void render();
	void updateSimulation(float dt);

	HINSTANCE hInstance;
	HWND hWnd;

	std::unique_ptr<Graphics> graphics;

	Comptr<ID3D11Buffer> vertex_buffer, index_buffer, matrix_buffer, instance_buffer;
	Comptr<ID3D11VertexShader> v_shader;
	Comptr<ID3D11PixelShader> p_shader;
	Comptr<ID3D11InputLayout> input_layout;

	Camera camera;
	float stime;
};
