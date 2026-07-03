#define _CRT_SECURE_NO_WARNINGS
#include "math.h"
#include <Windows.h>
#include <cstdint>
#include <iostream>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;


enum class COLOURS : u32 {
	BLUE = 0x000000FF,
	GREEN = 0x0000FF00,
	RED = 0x00FF0000
};
struct Framebuffer {
	BITMAPINFO bmInfo;
	i32 width;
	i32 height;
	i32 pitch;
	void* memory;
};

struct WindowSize {
	LONG windowWidth;
	LONG windowHeight;
};

struct WindowData {
	HWND hwnd;
	HDC deviceContext;
	RECT windowRect;
	WindowSize windowSize;

	WindowData(HWND _hwnd) {
		hwnd = _hwnd;
		deviceContext = GetDC(hwnd);
		GetClientRect(hwnd, &windowRect);
		windowSize.windowWidth = windowRect.right - windowRect.left;
		windowSize.windowHeight = windowRect.bottom - windowRect.top;
	}
	~WindowData() {
		ReleaseDC(hwnd, deviceContext);
	}

	static WindowSize GetWindowSize(HWND _hwnd) {
		RECT winRect;
		GetClientRect(_hwnd, &winRect);
		LONG winWidth = winRect.right - winRect.left;
		LONG winHeight = winRect.bottom - winRect.top;
		return WindowSize{ winWidth,winHeight };
	}
};

struct Triangle {
	Vector2 v0;
	Vector2 v1;
	Vector2 v2;
};
struct Face { int a, b, c; };


static Framebuffer buffer;
static bool running = true;
static float focalLength = 2500;
int centreScreenX, centreScreenY;
static float scale = -450;

void DrawLine(u32 x0, u32 y0, u32 x1, u32 y1);
inline void render(HWND hwnd);
void DrawTriangle(Triangle t);

struct Model {
	std::vector<Vector3> verts;
	std::vector<Face> faces;


	Model(const std::string& filename) {

		//loads whole file into in memory storage then parses

		FILE* fileHandle = fopen(filename.c_str(), "r");
		if (!fileHandle) { return; }
		fseek(fileHandle, 0, SEEK_END);
		long tell = ftell(fileHandle);
		char* buf = new char[tell + 1];
		buf[tell] = '\0';
		fseek(fileHandle, 0, SEEK_SET);
		size_t bytesRead = fread(buf, 1, tell, fileHandle);
		fclose(fileHandle);
		if (bytesRead != tell) {
			OutputDebugStringA("bytesRead != tell");
		}


		char* cur = buf;
		// parse line by line
		while (*cur != '\0') {
			char* lineStart = cur;

			while (*cur != '\n' && *cur != '\0') {
				cur++;
			}
			if (*cur == '\n') {
				*cur = '\0';
				cur++;
			}
			if (lineStart[0] == 'v' && lineStart[1] == ' ') {
				Vector3 vertex{};
				int matched = sscanf(lineStart, "v %f %f %f", &vertex.x, &vertex.y, &vertex.z);
				if (matched == 3) {
					verts.push_back(vertex);
				}
			}
			else if (lineStart[0] == 'f' && lineStart[1] == ' ') {
				Face face{};

				int a, b, c;
				int matched = sscanf(lineStart, "f %d/%*d/%*d %d/%*d/%*d %d/%*d/%*d", &a, &b, &c);
				if (matched == 3) {
					face.a = a; face.b = b; face.c = c;
					face.a--; face.b--; face.c--;
					faces.push_back(face);
				}
			}
		}
		delete[] buf;

	}

};

void ResizeBitmap(Framebuffer& buffer, u32 width, i32 height) {

	if (buffer.memory) {
		// if we have memory.. free it because ? : we want a new amount of memory to the new size
		VirtualFree(buffer.memory, 0, MEM_RELEASE);
	}
	buffer.width = width;
	buffer.height = height;

	buffer.bmInfo.bmiHeader.biSize = sizeof(BITMAPINFO);
	buffer.bmInfo.bmiHeader.biWidth = width;
	buffer.bmInfo.bmiHeader.biHeight = height; // previously we flipped it, because by default bitmap format 0,0 is the bottom LEFT for some reason? we want top right.
	buffer.bmInfo.bmiHeader.biPlanes = 1;
	buffer.bmInfo.bmiHeader.biBitCount = 32; // 8 bits padding, rgb is 24
	buffer.bmInfo.bmiHeader.biCompression = BI_RGB;


	u32 bytesPerPixel = 4;
	u32 size = width * height * bytesPerPixel;

	buffer.memory = VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	buffer.pitch = width * bytesPerPixel; // how long is a row?
}

void DisplayBitmapInWindow(WindowData& winData, Framebuffer& buffer) {
	StretchDIBits(winData.deviceContext, 0, 0, winData.windowSize.windowWidth, winData.windowSize.windowHeight, 0, 0,
		buffer.width, buffer.height, buffer.memory, &buffer.bmInfo, DIB_RGB_COLORS, SRCCOPY);
}

void PutPixel(i32 x, i32 y, u32 color);
void FramebufferRenderColour(u32 color) {
	// for each pixel, set the 8 green bits on and turn everything else off
	// RR GG BB AA
	// AA BB GG RR
	// loop height->width because we go row -> each column entry -> next row?
	u32* pixels = (u32*)buffer.memory;
	for (u32 y = 0; y < buffer.height; y++) {
		for (u32 x = 0; x < buffer.width; x++) {
			pixels[(y * buffer.width) + x] = color; // green
		}
	}
	
	//for (u32 y = 0; y < buffer.height; y++) {
	//	for (u32 x = 0; x < buffer.width; x++) {
	//		PutPixel(x, y, color);
	//	}
	//}
}	





void ClearScreen() {
	FramebufferRenderColour(0x00000000);
}



static u32 winwidth = 2560;
static u32 winheight = 1440;

struct Transform {
	Vector4 position;
	Vector4 scale;
	Vector4 rotation;
};
static  Transform camera{};

Matrix4D make_perspective();
Matrix4D MakeViewMatrix();


static Matrix4D p = make_perspective();
static Matrix4D v = MakeViewMatrix();
LRESULT WndProc(HWND hwnd, UINT MSG, WPARAM wParam, LPARAM lParam) {
	LRESULT result = 0;
	switch (MSG) {
	case WM_ACTIVATEAPP: {
		OutputDebugStringA("WM_ACTIVATEAPP\n");
	}break;
	case WM_NCCREATE: {
		//CREATESTRUCT* cs = (CREATESTRUCT*)lParam;
		//int** p = (int**)cs->lpCreateParams;
		//SetWindowLongPtrA(hwnd, GWLP_USERDATA, (LONG_PTR)p);
		result = 1; // special message, if 0 is returned, then window creation fails.
	}break;
	case WM_SYSKEYDOWN: {
		u32 VKCode = wParam;
		if (VKCode == VK_F4) {
			OutputDebugStringA("alt f4 pressed");
			running = false;
		}
	}break;

	case WM_KEYDOWN: {
		u32 VKCode = wParam;
		bool WasDown = ((lParam & (1 << 30)) != 0); // if true - this is a auto-repeat (key held down) stroke, false = initial stroke
		bool IsDown = ((lParam & (1 << 31)) == 0); // check current state of the key - true = currently being pressed false = not being pressed

		if (VKCode == 'O') {
			camera.position.y -= 0.1;
			v = MakeViewMatrix();
		}
		if (VKCode == 'P') {
			camera.position.y += 0.1;
			v = MakeViewMatrix();
		}
		if (VKCode == 'K') {
			camera.position.x -= 0.1;
			v = MakeViewMatrix();
		}
		if (VKCode == 'L') {
			camera.position.x += 0.1;
			v = MakeViewMatrix();
		}
		if (VKCode == 'N') {
			camera.position.z -= 0.1;
			v = MakeViewMatrix();
		}
		if (VKCode == 'M') {
			camera.position.z += 0.1;
			v = MakeViewMatrix();
		}
	}break;
	//case WM_SIZE: {
	//	// just handle resize in the render function, by computing a new size each tick idc
	//	//WindowData windowData(hwnd);
	//	//ResizeDIBSection(GlobalBackBuffer, windowData.windowSize.windowWidth,windowData.windowSize.windowHeight);
	//	//OutputDebugStringA("WM_SIZE\n");
	//	//RenderGradient(&GlobalBackBuffer,**var, **(var + 1));
	//}break;
	case WM_DESTROY: {
		running = false;
	}break;

	case WM_CLOSE: {
		running = false;
	}break;

	case WM_PAINT: {
		PAINTSTRUCT paint;
		HDC deviceContext = BeginPaint(hwnd, &paint);
		int x = paint.rcPaint.left;
		int y = paint.rcPaint.top;

		LONG width = paint.rcPaint.right - paint.rcPaint.left;
		LONG height = paint.rcPaint.top - paint.rcPaint.bottom;
		WindowData winData(hwnd);
		WindowSize winSize = winData.windowSize;
		if (winSize.windowWidth != buffer.width ||
			winSize.windowHeight != buffer.height) {
			ResizeBitmap(buffer, winSize.windowWidth, winSize.windowHeight);
			centreScreenX = winSize.windowWidth / 2;
			centreScreenY = winSize.windowHeight / 2;



		}
		//PutPixel(100, 200, (u32)COLOURS::RED);
	

		DisplayBitmapInWindow(winData, buffer);
		EndPaint(hwnd, &paint);
	}break;
	default: {
		//OutputDebugStringA("DefWindProc");
		result = DefWindowProc(hwnd, MSG, wParam, lParam);
	}
	}
	return result;
}



//DDA
void DrawLine(
	i32 x0,
	i32 y0,
	i32 x1,
	i32 y1
)
{

	// get the deltas - how far along each axis we move
	float dx =
		x1 - x0;

	float dy =
		y1 - y0;

	// get the max steps - to ensure we dont skip a pixel, we get the maximum amount of movements we need to do
	float steps =
		max( 
			abs(dx),
			abs(dy)
		);

	// figure out how much to increment each axis by each tick, so we can fractionally move toward our goal
	// the slope is hidden in here. slope = dy / dx - tan(angleOfLine). DDA preserves the slope
	float xInc =
		dx / steps;

	float yInc =
		dy / steps;

	float x = x0;
	float y = y0;

	for (
		int i = 0;
		i <= steps; // <= to include the endpoint
		i++
		)
	{
		PutPixel( 
			(int)std::roundf(x), // must round here, if u round in the acclumulation, it will reset the fractional progress of that tick
			(int)std::roundf(y),
			(u32)COLOURS::BLUE
		);

		x += xInc;
		y += yInc;
	}
}





//Vector2 Project(Vector3 v) {
//	//if (IsNearlyZero(v.z, 0.01f)) {
//	//	return { INFINITY,INFINITY };
//	//}
//	float z = v.z+5.0f;
//	if (z <= 0.01f) {
//		return { INFINITY,INFINITY };
//	}
//	return {
//		(v.x) / v.z * focalLength + (centreScreenX),
//		(v.y) / v.z * focalLength + (centreScreenY)
//	};
//}




//Vector2 ProjectOrtho(Vector3 v) {
//	// Move model in front of camera
//	float cameraZ = -5.0f;        // camera at z = 0 looking along +Z
//	float z = v.z - cam.z;      // z relative to camera
//	if (z <= 0.01f) return { INFINITY, INFINITY };
//
//	float x = v.x - cam.x;
//	float y = v.y - cam.y;
//
//	float screenX = (x) * scale + centreScreenX;
//	float screenY = (y) * scale + centreScreenY;
//
//	return { screenX, screenY };
//}
Vector2 Project(Vector3 v) {
	float screenX = (v.x / v.z) * focalLength;
	float screenY = (v.y / v.z) * focalLength;
	return { screenX, screenY };
}

struct Object {
	Model* model; // heavy ? use pointer.. can i even copy it... ? will perform a copy on the vec
	Transform transform;
};

constexpr float NEAR_PLANE = 1.f;
Vector4 RIGHT{ 1.f,0,0,0 };
Vector4 UP{ 0,1.f,0,0 };
Vector4 FORWARD{ 0,0,1.f,0 };



// call this when the fov/aspect/camera pos or rot changes
Matrix4D MakeViewMatrix() {
	Matrix4D rot = Matrix4D::Rz(camera.rotation.z) * Matrix4D::Ry(camera.rotation.y) * Matrix4D::Rx(camera.rotation.x);
	Matrix4D rotInv = rot.Transpose();
	Vector4 translation = (rotInv * camera.position);
	Matrix4D view(rotInv[0], rotInv[1], rotInv[2], translation);
	return view;
}

// call this per model
Matrix4D MakeModelMatrix(Transform transform) {
	Matrix4D local(RIGHT, UP, FORWARD, { 0,0,0,1 });
	Matrix4D scale(transform.scale);
	Matrix4D translate( // set translation offset to last column
		Vector4{ 1,0,0,0 },
		Vector4{ 0,1,0,0 },
		Vector4{ 0,0,1,0 },
		Vector4{
			transform.position.x,
			transform.position.y,
			transform.position.z,
			1
		}
	);
	Matrix4D rotation = Matrix4D::Rz(transform.rotation.z) * Matrix4D::Ry(transform.rotation.y) * Matrix4D::Rx(transform.rotation.x);
	Matrix4D world = translate * rotation * scale;
	return world;
}
Vector4 LocalToWorld(Vector4 v, Transform transform) {
	Matrix4D local(RIGHT,UP,FORWARD,{0,0,0,1});
	Matrix4D scale(transform.scale);
	Matrix4D translate( // set translation offset to last column
		Vector4{ 1,0,0,0 },
		Vector4{ 0,1,0,0 },
		Vector4{ 0,0,1,0 },
		Vector4{
			transform.position.x,
			transform.position.y,
			transform.position.z,
			1
		}
	);
	Matrix4D rotation = Matrix4D::Rz(transform.rotation.z) * Matrix4D::Ry(transform.rotation.y) * Matrix4D::Rx(transform.rotation.x);
	Matrix4D world = translate * rotation * scale;
	Vector4 v_world = world * v;
	return v_world;
}
//Vector4 WorldToView(Vector4 v) {
//	// to achieve the view of the camera being fixed at the origin, we subtract the cameras position from every vert
//	Matrix4D view(
//		Vector4{ RIGHT.x, UP.x, FORWARD.x, 0 },
//		Vector4{ RIGHT.y, UP.y, FORWARD.y, 0 },
//		Vector4{ RIGHT.z, UP.z, FORWARD.z, 0 },
//		Vector4{
//			-Dot(RIGHT, cam),
//			-Dot(UP, cam),
//			-Dot(FORWARD, cam),
//			1
//		}
//	);
//	Vector4 v_view = view * v;
//	return v_view;
//
//
//	// build rotation matrix for camera
//	Matrix4D rot = Matrix4D::Rz(camera.rotation.z) * Matrix4D::Ry(camera.rotation.y) * Matrix4D::Rx(camera.rotation.x);
//	Matrix4D rotInv = rot.Transpose();
//	Vector3 translation = -(rotInv * camera.position);
//	Matrix4D view(rotInv[0], rotInv[1], rotInv[2], translation);
//
//}
Matrix4D make_perspective() {
	float fov = degrees_to_radians(45.6);
	float tanHalfFovy = std::tanf(fov / 2);
	// f = fov scaling factor. we mul our x and y by this f to either shrink or grow objects depending on the fov(zoom factor)
	float f = 1 / tanHalfFovy;

	float& d = f;

	float ar = (float)winheight / (float)winwidth;

	float zNear = 1.0f;
	float zFar = 10.f;

	float zRange = zNear - zFar;

	float a = (-zFar - zNear) / zRange;
	float b = (2 * zFar * zNear / zRange);

	float lam = zFar / (zFar - zNear);
	float lam2 = (-zFar * zNear) / (zFar - zNear);

	Matrix4D projection({ f * ar,0,0,0 },
		{ 0,f,0,0 },
		{ 0,0,lam,lam2 },
		{ 0,0,-1,0 }
	);
	return projection;
}
Vector2 no_mat4_construction_view_to_screen(Vector4 v,Matrix4D& projection) {
	Vector4 v_project = projection * v;
	// now we figure out NDCS
	float ndcX = v_project.x / v_project.w;
	float ndcY = v_project.y / v_project.w;

	float screenX1 = ((ndcX + 1.0f) * 0.5f * winwidth);
	float screenY1 = ((1.0f - ndcY) * 0.5f * winheight);

	float screenX = (v.x / v.z) * focalLength + centreScreenX;
	float screenY = (v.y / v.z) * focalLength + centreScreenY;
return { screenX1, screenY1 };
}
Vector2 ViewToScreen(Vector4 v) {


	float fov = degrees_to_radians(19.6);
	float tanHalfFovy = std::tanf(fov / 2);
	// f = fov scaling factor. we mul our x and y by this f to either shrink or grow objects depending on the fov(zoom factor)
	float f = 1 / tanHalfFovy;

	float& d = f;

	float ar = (float)winheight / (float)winwidth;

	float zNear = 1.0f;
	float zFar = 10.f;

	float zRange = zNear - zFar;

	float a = (-zFar - zNear) / zRange;
	float b = (2 * zFar * zNear / zRange);

	float lam = zFar / (zFar - zNear);
	float lam2 = (-zFar * zNear) / (zFar - zNear);

	Matrix4D projection({ f*ar,0,0,0 },
						{ 0,f,0,0 },
						{ 0,0,lam,lam2 }, 
						{ 0,0,-1,0 }
	);


	Vector4 v_project = projection * v;


	// now we figure out NDCS
	float ndcX = v_project.x / v_project.w;
	float ndcY = v_project.y / v_project.w;
	float ndcZ = v_project.z / v_project.w;

	// depth test

	float screenX1 = ((ndcX + 1.0f) * 0.5f * winwidth);
	float screenY1 = ((1.0f - ndcY) * 0.5f * winheight);

	float screenX = (v.x / v.z) * focalLength + centreScreenX;
	float screenY = (v.y / v.z) * focalLength + centreScreenY;
	return { screenX1, screenY1 };
}


Vector2 NDCToScreen(Vector4 v) {
	float screenX1 = ((v.x + 1.0f) * 0.5f * winwidth);
	float screenY1 = ((1.0f - v.y) * 0.5f * winheight);
	return { screenX1, screenY1 };
}

void RenderModels(std::vector<Object>& models) {

	for (const auto& m : models) {
		auto& faces = m.model->faces;
		Transform transform = m.transform;
		Matrix4D model = MakeModelMatrix(transform);
		Matrix4D MVP = p * v * model;
		for (const auto& face : faces) {

			Vector4 v0{m.model->verts[face.a],1}; // THESE ARE IN LOCAL SPACE
			Vector4 v1{m.model->verts[face.b],1};
			Vector4 v2{m.model->verts[face.c],1};
			

			auto v0clip = MVP * v0;
			auto v1clip = MVP * v1;
			auto v2clip = MVP * v2;

			// apply transform? this means go from LOCAL -> WORLD. just position for now.
			
			//auto v0_world = LocalToWorld(v0, transform);
			//auto v1_world = LocalToWorld(v1, transform);
			//auto v2_world = LocalToWorld(v2, transform);
			//// apply camera / view transform -> for now just subtract camera
			//auto v0_view = WorldToView(v0_world);
			//auto v1_view = WorldToView(v1_world);
			//auto v2_view = WorldToView(v2_world);
			
			if (v0clip.z > NEAR_PLANE && v1clip.z > NEAR_PLANE && v2clip.z > NEAR_PLANE) {
				// apply projection
				// get screen coordinates
		/*		Vector2 screen0 = ViewToScreen(v0_view);
				Vector2 screen1 = ViewToScreen(v1_view);
				Vector2 screen2 = ViewToScreen(v2_view);*/

				//Vector2 screen0 = no_mat4_construction_view_to_screen(v0_view,projection_matrix);
				//Vector2 screen1 = no_mat4_construction_view_to_screen(v1_view,projection_matrix);
				//Vector2 screen2 = no_mat4_construction_view_to_screen(v2_view,projection_matrix);

				v0clip /= v0clip.w;
				v1clip /= v1clip.w;
				v2clip /= v2clip.w;

				auto screen0 = NDCToScreen(v0clip);
				auto screen1 = NDCToScreen(v1clip);
				auto screen2 = NDCToScreen(v2clip);

				// populate a triangle and draw it
				Triangle tri{ screen0,screen1,screen2 };
				DrawTriangle(tri);
			}
		}
	}
}
void DrawModel(const Model& model) {
	/*
	for each face, get corresponding verts	
	Vec3 v0 = verts[a];

	project verts 3D->2D
	Vec3 p0 = project(v0)

	Draw with projected verts
	Triangle t = {p0,p1,p2}
	DrawTriangle(t)
	*/

	for (const auto& face : model.faces) {
		auto& v0 = model.verts[face.a];
		auto& v1 = model.verts[face.b];
		auto& v2 = model.verts[face.c];

		Vector2 p0 = Project(v0);
		Vector2 p1 = Project(v1);
		Vector2 p2 = Project(v2);

		//if (p0.x == INFINITY) { 
		//		continue;
		//}
		//if (p1.x == INFINITY) { continue; }
		//if (p2.x == INFINITY) { continue; }

		Triangle t{ p0,p1,p2 };
		DrawTriangle(t);
	}
}

void DrawTriangle(Triangle t) {

	if (t.v0.x == INFINITY || t.v1.x == INFINITY || t.v2.x == INFINITY) {
		return;
	}
	DrawLine(
		(i32) t.v0.x,
		(i32) t.v0.y,
		(i32) t.v1.x,
		(i32) t.v1.y
	);

	DrawLine(
		(i32) t.v1.x,
		(i32) t.v1.y,
		(i32) t.v2.x,
		(i32) t.v2.y
	);

	DrawLine(
		(i32) t.v2.x,
		(i32) t.v2.y,
		(i32) t.v0.x,
		(i32) t.v0.y
	);
}

void PutPixel(i32 x, i32 y, u32 color)
{
	if (x < 0 || y < 0 || x >= buffer.width || y >= buffer.height){
		//OutputDebugStringA("OUT OF BOUNDS");
	return;
}

	u32* pixels = (u32*)buffer.memory;
	pixels[y * buffer.width + x] = color; 
}


inline void render(HWND hwnd) {
	WindowData frameWinData(hwnd);
	if (frameWinData.windowSize.windowWidth != buffer.width ||
		frameWinData.windowSize.windowHeight != buffer.height) {
		ResizeBitmap(buffer, frameWinData.windowSize.windowWidth, frameWinData.windowSize.windowHeight);
		centreScreenX = frameWinData.windowSize.windowWidth / 2;
		centreScreenY = frameWinData.windowSize.windowHeight / 2;

		
	}
	DisplayBitmapInWindow(frameWinData, buffer);
}


int WINAPI wWinMain(HINSTANCE Instance, HINSTANCE PrevInstance, LPWSTR lpCmdLine, int CmdShow) {


	Model model("Makima.obj");
	//Model model2b("2B.obj");

	Object object;
	object.model = &model;

	Object object2;
	//object2.model = &model2b;

	Transform transform;
	transform.position = { 1,1,1,1 };
	transform.scale = { 1,1,1,1 };
	transform.rotation = { 0,0,0,0 };
	object.transform = transform;

	//Transform transform2;
	//transform2.position = { 1,0,0,1 };
	//object2.transform = transform2;

	std::vector<Object> objects;
	objects.push_back(object);
	//objects.push_back(object2);

	//Model model1("diablo3.obj");
	//static u32 winwidth = 2560;
	//static u32 winheight = 1440;


	camera.position = Vector4{ 0,0,0,0 };
	camera.rotation = Vector4{ 0,0,0,0 };

	
	




	WNDCLASSEX windowclass{};
	windowclass.style = CS_HREDRAW | CS_VREDRAW;
	windowclass.hInstance = Instance;
	windowclass.lpszClassName = L"WindowClass";
	windowclass.lpfnWndProc = WndProc;
	windowclass.cbSize = sizeof(WNDCLASSEX);
	HRESULT windowResult = RegisterClassEx(&windowclass);
	if (!windowResult) { OutputDebugStringA("Failed to register window class"); return 0; }
	HWND hwnd = CreateWindowExW(0, windowclass.lpszClassName, L"Noelle CPU Renderer",
		WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, winwidth, winheight, 0, 0, Instance, 0);

	//Show & Update Window
	if (!hwnd) {
		OutputDebugStringA("window creation failed");
		return 0;
	}
	ShowWindow(hwnd, CmdShow);
	UpdateWindow(hwnd);

	WindowData winData(hwnd);
	ResizeBitmap(buffer, winwidth, winheight);
	

	i64 lastCycleCount = __rdtsc();
	LARGE_INTEGER frequency;
	QueryPerformanceFrequency(&frequency);
	LARGE_INTEGER lastCounter;
	QueryPerformanceCounter(&lastCounter);
	MSG msg{};



	while (running) {
		//Timer timer;
		if (msg.message == WM_QUIT) {
			running = false;
		}
		// remove each message
		while (PeekMessageW(&msg, 0, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}
		// render
		// every frame need to a) check the size of the window, if changed then call ResizeBitmap then b) put the bitmap in the window. so thats StretchDIBits
		//FramebufferRenderColour((u32)COLOURS::GREEN);
		
		


		objects[0].transform.rotation.y += 0.1;
		ClearScreen();
		RenderModels(objects);
		render(hwnd);


		
		// perf
		LARGE_INTEGER endCounter;
		QueryPerformanceCounter(&endCounter);

		i64 endCycleCount = __rdtsc();
		i64 totalCycles = endCycleCount - lastCycleCount;


		double frameTime = ((double)(endCounter.QuadPart - lastCounter.QuadPart) / (double)frequency.QuadPart);
		float FPS = 1.0f / frameTime;
		char buffer[256];

		
		sprintf_s(buffer, 256, "FPS : %.3f  \n Mega Cycles taken for this frame: %d \n ", FPS, totalCycles / (1000 * 1000));
		OutputDebugStringA(buffer);

		lastCycleCount = endCycleCount;
		lastCounter = endCounter;
	}
}


