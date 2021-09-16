#include <string>
#include <d3dcompiler.h>

#include "Application.h"
#include "Math3D.h"

#pragma comment(lib, "d3dcompiler.lib")

using namespace Math3D;


bool Application::Init(HINSTANCE hInstance)
{
	WNDCLASS wc = { 0 };
	wc.cbWndExtra = sizeof(this);
	wc.hCursor = LoadCursor(0, IDC_ARROW);
	wc.hInstance = hInstance;
	wc.lpszClassName = "Mainwnd";
	wc.lpfnWndProc = sWndProc;

	RegisterClass(&wc);

	bool fullscreen = false;
	std::string title = "Voxel Water";

	if (fullscreen)
	{
		unsigned width = 800;
		unsigned height = 600;
		hWnd = CreateWindow(wc.lpszClassName, title.c_str(), WS_POPUP, 0, 0, width, height, 0, 0, hInstance, this);
	}
	else
	{
		unsigned windowstyle = WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
		int xpos = CW_USEDEFAULT;
		int ypos = CW_USEDEFAULT;
		unsigned width = 800;
		unsigned height = 600;

		RECT r;
		SetRect(&r, 0, 0, width, height);
		AdjustWindowRect(&r, windowstyle, 0);
		hWnd = CreateWindow(wc.lpszClassName, title.c_str(), windowstyle, xpos, ypos, r.right - r.left, r.bottom - r.top, 0, 0, hInstance, this);
	}

	if (!hWnd)
	{
		return false;
	}

	return initGraphics();
}


void Application::Run()
{
	ShowWindow(hWnd, SW_SHOWNORMAL);

	DWORD lasttime = GetTickCount();

	MSG Msg;
	do
	{
		while (PeekMessage(&Msg, 0, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&Msg);
			DispatchMessage(&Msg);
		}

		unsigned newtime = GetTickCount();
		unsigned diff = newtime - lasttime;
		lasttime = newtime;

		updateSimulation(static_cast<float>(diff) / 1000.f);
		render();
	}
	while (Msg.message != WM_QUIT);
}


LRESULT CALLBACK Application::sWndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	if (Msg == WM_NCCREATE)
	{
		SetWindowLongPtr(hWnd, 0, reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCT *>(lParam)->lpCreateParams));
	}
	Application *app = reinterpret_cast<Application *>(GetWindowLongPtr(hWnd, 0));
	if (app)
	{
		return app->WndProc(hWnd, Msg, wParam, lParam);
	}
	return DefWindowProc(hWnd, Msg, wParam, lParam);
}


LRESULT CALLBACK Application::WndProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	switch (Msg)
	{
	/*case WM_MOUSEMOVE:
		{
			if (active)
			{
				int xdiff = LOWORD(lParam) - lastpos.x;
				int ydiff = HIWORD(lParam) - lastpos.y;
				if (xdiff || ydiff)
				{
					if (GetFocus() == hWnd)
						CenterCursorPos();
					inputmanager->AddMouseMovement(xdiff, ydiff);
				}
			}
			else
			{
				lastpos.x = LOWORD(lParam);
				lastpos.y = HIWORD(lParam);
			}
		}
		break;
	case WM_KEYDOWN:
		inputmanager->AddInput(LOWORD(wParam), true);
		break;
	case WM_KEYUP:
		inputmanager->AddInput(LOWORD(wParam), false);
		break;
	case WM_LBUTTONDOWN:
		inputmanager->AddInput(VK_LBUTTON, true);
		break;
	case WM_LBUTTONUP:
		inputmanager->AddInput(VK_LBUTTON, false);
		break;
	case WM_RBUTTONDOWN:
		inputmanager->AddInput(VK_RBUTTON, true);
		break;
	case WM_RBUTTONUP:
		inputmanager->AddInput(VK_RBUTTON, false);
		break;*/
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	}
	return DefWindowProc(hWnd, Msg, wParam, lParam);
}


bool Application::initGraphics()
{
	graphics = std::make_unique<Graphics>();
	if (!graphics->init(hWnd))
		return false;

	if (!initShaders())
		return false;

	if (!initGeometry())
		return false;

	RECT rect;
	GetClientRect(hWnd, &rect);
	float width = static_cast<float>(rect.right - rect.left);
	float height = static_cast<float>(rect.bottom - rect.top);

	camera.SetCameraMode(Camera::CameraModeFPS);
	camera.SetAspect(width / height);
	camera.SetFOVY(ToRadian(60));
	camera.SetNearPlane(1.f);
	camera.SetFarPlane(300.f);
	camera.SetMinXAngle(ToRadian(80.f));
	camera.SetMaxXAngle(ToRadian(80.f));
	camera.SetRoll(0.f);

	return true;
}


bool Application::initShaders()
{
	auto v_code = R"r1y(
		struct vs_input
		{
			float3 pos : POSITION;
			float3 color : COLOR;
		};

		struct vs_output
		{
			float4 pos : SV_POSITION;
			float3 color : COLOR;
		};

		cbuffer matrices : register(b0)
		{
			float4x4 world, view, proj;
		};

		void vs_main(vs_input input, out vs_output output)
		{
			float4 worldpos = float4(input.pos, 1.f);
			output.pos = mul(proj, mul(view, mul(world, worldpos)));
			output.color = input.color;
		}
	)r1y";

	auto p_code = R"r1y(
		struct ps_input
		{
			float4 pos : SV_POSITION;
			float3 color : COLOR;
		};

		struct ps_output
		{
			float4 color : SV_TARGET;
		};

		void ps_main(ps_input input, out ps_output output)
		{
			output.color = float4(input.color, 1.f);
		};
	)r1y";

	UINT flags =
#ifdef _DEBUG
		D3DCOMPILE_DEBUG |
#endif
		0;

	Comptr<ID3DBlob> v_compiled, v_error;
	D3DCompile(v_code, strlen(v_code), "vshader", nullptr, nullptr, "vs_main", "vs_5_0", flags, 0, &v_compiled, &v_error);
	if (v_error)
	{
		auto title = v_compiled ? "Warning" : "Error";
		auto style = v_compiled ? MB_ICONWARNING : MB_ICONERROR;
		MessageBox(0, static_cast<const char *>(v_error->GetBufferPointer()), title, style);
	}
	if (!v_compiled)
	{
		return false;
	}

	Comptr<ID3DBlob> p_compiled, p_error;
	D3DCompile(p_code, strlen(p_code), "pshader", nullptr, nullptr, "ps_main", "ps_5_0", flags, 0, &p_compiled, &p_error);
	if (p_error)
	{
		auto title = p_compiled ? "Warning" : "Error";
		auto style = p_compiled ? MB_ICONWARNING : MB_ICONERROR;
		MessageBox(0, static_cast<const char *>(p_error->GetBufferPointer()), title, style);
	}
	if (!p_compiled)
	{
		return false;
	}

	auto device = graphics->GetDevice();
	HRESULT hr = device->CreateVertexShader(v_compiled->GetBufferPointer(), v_compiled->GetBufferSize(), 0, &v_shader);
	if (FAILED(hr))
		return false;

	hr = device->CreatePixelShader(p_compiled->GetBufferPointer(), p_compiled->GetBufferSize(), 0, &p_shader);
	if (FAILED(hr))
		return false;
	
	auto context = graphics->GetContext();
	context->VSSetShader(v_shader, nullptr, 0);
	context->PSSetShader(p_shader, nullptr, 0);

	// build the input assembler
	D3D11_INPUT_ELEMENT_DESC input_desc[] =
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"COLOR"   , 0, DXGI_FORMAT_R8G8B8A8_UNORM , 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
	};

	hr = device->CreateInputLayout(input_desc, std::size(input_desc), v_compiled->GetBufferPointer(), v_compiled->GetBufferSize(), &input_layout);
	if (FAILED(hr))
		return false;

	context->IASetInputLayout(input_layout);

	return true;
}


bool Application::initGeometry()
{
	auto device = graphics->GetDevice();
	
	D3D11_BUFFER_DESC vertex_buffer_desc = { 0 };
	vertex_buffer_desc.StructureByteStride = sizeof(Vertex);
	vertex_buffer_desc.ByteWidth = sizeof(Vertex) * 3;
	vertex_buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vertex_buffer_desc.Usage = D3D11_USAGE_DEFAULT;
	
	Vertex vertices[3] =
	{
		{-1.f, -1.f, 0.5f, 0x000000ff}, // R
		{0.f, 1.f, 0.5f, 0x0000ff00}, // G
		{1.f, -1.f, 0.5f, 0x00ff0000}, // B
	};

	D3D11_SUBRESOURCE_DATA vertex_init_data = { 0 };
	vertex_init_data.pSysMem = vertices;
	HRESULT hr = device->CreateBuffer(&vertex_buffer_desc, &vertex_init_data, &vertex_buffer);
	if (FAILED(hr))
		return false;

	D3D11_BUFFER_DESC cbuffer_desc = { 0 };
	cbuffer_desc.ByteWidth = sizeof(matrix_cbuffer);
	cbuffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbuffer_desc.Usage = D3D11_USAGE_DYNAMIC;
	cbuffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	hr = device->CreateBuffer(&cbuffer_desc, nullptr, &matrix_buffer);
	if (FAILED(hr))
		return false;

	return true;
}


void Application::render()
{
	float clear_color[4] = {0.2f, 0.f, 0.f, 0.f};
	auto ctx = graphics->GetContext();
	ctx->ClearRenderTargetView(graphics->GetMainRendertargetView(), clear_color);
	ctx->ClearDepthStencilView(graphics->GetMainDepthStencilView(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.f, 0);

	UINT v_strides = sizeof(Vertex), v_offsets = 0;
	ctx->IASetVertexBuffers(0, 1, &vertex_buffer, &v_strides, &v_offsets);
	ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	camera.SetEye(Vector3(2.f, 4.f, -10.f));
	camera.SetLookat(Vector3(0.f, 0.f, 0.f));

	D3D11_MAPPED_SUBRESOURCE sub;
	ctx->Map(matrix_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &sub);
	matrix_cbuffer *matrices = static_cast<matrix_cbuffer *>(sub.pData);
	matrices->world = Matrix4x4::IdentityMatrix();
	matrices->view = camera.GetViewMatrix();
	matrices->proj = camera.GetProjectionMatrix();
	ctx->Unmap(matrix_buffer, 0);
	ctx->VSSetConstantBuffers(0, 1, &matrix_buffer);

	ctx->Draw(3, 0);

	graphics->Present();
	graphics->WaitForVBlank();
}


void Application::updateSimulation(float dt)
{
}
