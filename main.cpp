//+X = right
//+ Y = up
//- Z = forward
#define STB_IMAGE_IMPLEMENTATION
#define NOMINMAX // GO AWAY WINDOWS
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN


#include "math.h"
#include <Windows.h>
#include <cstdint>
#include <iostream>
#include "stb_image.h"



using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

using i8 = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;

enum class COLOURS : u32 {
	BLUE = 0x000000FF,
	GREEN = 0x0000FF00,
	RED = 0x00FF0000,
	WHITE = 0xFFFFFFFF,
	SKIN = 0x00F7DCCB
};
struct Framebuffer {
	BITMAPINFO bmInfo;
	i32 width;
	i32 height;
	i32 pitch;
	void* memory;
	void* depth;
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
struct Transform {
	Vector4 position;
	Vector4 scale;
	Vector4 rotation;
};
struct Face { int a, b, c; };


static Framebuffer buffer;
static bool running = true;
static float focalLength = 2500;
int centreScreenX, centreScreenY;
static float scale = -450;
static u32 winwidth = 1280;
static u32 winheight = 720;


static  Transform camera{};
static Matrix4D p;
static Matrix4D v;


Matrix4D MakePerspective(float fovDeg);
Matrix4D MakeViewMatrix();
void DrawLine(u32 x0, u32 y0, u32 x1, u32 y1);
inline void render(HWND hwnd);
void DrawTriangle(Triangle t);


struct Vertex {
	Vector4 pos; // w = 1
	Vector2 uv; // w = 0
	Vector4 normal;
	Vector4 colour;
};

#include <filesystem>
#include <ostream>
#include <fstream>
#include <charconv>
#include <iostream>
#include <string_view>


std::vector<float> parse_floats_from_line(std::string_view line) {
	std::vector<float> vals;

	while (!line.empty()) {
		while (!line.empty() &&
			(std::isspace(static_cast<unsigned char>(line.front())) ||
				std::isalpha(static_cast<unsigned char>(line.front())) ||
				(std::ispunct(static_cast<unsigned char>(line.front())) && line.front() != '-')))
		{
			line.remove_prefix(1);
		}

		if (line.empty()) {
			break;
		}

		float value;
		auto [ptr, errorcode] = std::from_chars(line.data(), line.data() + line.size(), value);

		if ((errorcode != std::errc())) {
			// TODO - HANDLE
			break;
		}
		vals.push_back(value);

		// advance the ptr past what we just parsed
		line.remove_prefix(ptr - line.data());
	}
	return vals;
}
std::vector<float> parse_ints_from_line(std::string_view line) {
	std::vector<float> vals;

	while (!line.empty()) {
		while (!line.empty() &&
			(std::isspace(static_cast<unsigned char>(line.front())) ||
				std::isalpha(static_cast<unsigned char>(line.front())) ||
				(std::ispunct(static_cast<unsigned char>(line.front())) && line.front() != '-')))
		{
			line.remove_prefix(1);
		}

		if (line.empty()) {
			break;
		}

		int value;
		auto [ptr, errorcode] = std::from_chars(line.data(), line.data() + line.size(), value);

		if ((errorcode != std::errc())) {
			// TODO - HANDLE
			break;
		}
		vals.push_back(value);

		// advance the ptr past what we just parsed
		line.remove_prefix(ptr - line.data());
	}
	return vals;
}
struct Texture {
	int width;
	int height;
	u32* data;
};

struct Model {
	std::vector<Vector3> verts;
	std::vector<Face> faces;

	std::vector<Vertex> vertices;
	std::vector<u32> indexBuffer;


	std::vector<Vector4> positions;
	std::vector<Vector3> normals;
	std::vector<Vector2> uvs;

	Texture texture;

	Model() {};

	Model(std::vector<Vector3> verts, std::vector<Face> faces) : verts(verts), faces(faces) {};
	explicit Model(std::filesystem::path path) {
		ReadObj(path);
	}
	void ReadObj(const std::filesystem::path& path) {
		int index = 1;
		//check if regular file
		bool isRegular = std::filesystem::is_regular_file(path);
		if (!isRegular) {
			// TODO - handle
		}

		std::ifstream file(path);

		// check if open
		if (!file.is_open()) {
			// TODO - handle
		}

		
		
		std::string line;
		while (std::getline(file, line)) {



			if (line.starts_with("#")) {
				continue;
			}
			if (line.empty()) {
				continue;
			}
			if (line.starts_with("v ")) {
				std::vector<float> vals = parse_floats_from_line(line);
				if (vals.size() < 3) {
					continue;
				}
				Vector4 pos = { vals[0],vals[1],vals[2],1 };
				positions.push_back(pos);
			}
			else if (line.starts_with("vt ")) {
				std::vector<float> vals = parse_floats_from_line(line);
				Vector2 pos = { vals[0],vals[1] };
				uvs.push_back(pos);
			}
			else if (line.starts_with("vn ")) {
				std::vector<float> vals = parse_floats_from_line(line);
				Vector3 pos = { vals[0],vals[1],vals[2] };
				normals.push_back(pos);
			}
			else if (line.starts_with("f ")) {
				// ok i take 9 values. each set of 3 represents the info for that corner. pos/uv/norm
				//Construct Vertex.
				//Push it into vertices.
				//Push vertices.size() - 1 into indexBuffer.


				std::vector<float> i = parse_ints_from_line(line);

				if (i.size() == 9) {
					// tri

				// parse corner a
					auto cornerAPositionIdx = i[0] - 1;
					auto cornerAUVIdx = i[1] - 1;
					auto cornerANormalIdx = i[2] - 1;
		
					Vertex vertex;
					vertex.pos = positions[cornerAPositionIdx];
					vertex.uv = uvs[cornerAUVIdx];
					vertex.normal = normals[cornerANormalIdx];

					vertices.push_back(vertex);
					indexBuffer.push_back(vertices.size() - 1);
					
					// parse corner b
					auto cornerBPositionIdx = i[3] - 1;
					auto cornerBUVIdx = i[4] - 1;
					auto cornerBNormalIdx = i[5] - 1;

					Vertex vertexB;
					vertexB.pos = positions[cornerBPositionIdx];
					vertexB.uv = uvs[cornerBUVIdx];
					vertexB.normal = normals[cornerBNormalIdx];

					vertices.push_back(vertexB);
					indexBuffer.push_back(vertices.size() - 1);
					// parse corner c
					auto cornerCPositionIdx = i[6] - 1;
					auto cornerCUVIdx = i[7] - 1;
					auto cornerCNormalIdx = i[8] - 1;




					Vertex vertexC;
					vertexC.pos = positions[cornerCPositionIdx];
					vertexC.uv = uvs[cornerCUVIdx];
					vertexC.normal = normals[cornerCNormalIdx];
					vertices.push_back(vertexC);
					indexBuffer.push_back(vertices.size() - 1);

				}
			}
		}

		/*
		for (int i = 0; i < indices.size(); i += 3)
			{
				uint32_t a = indices[i];
				uint32_t b = indices[i + 1];
				uint32_t c = indices[i + 2];

				vec3 A = positions[a];
				vec3 B = positions[b];
				vec3 C = positions[c];
			}
		*/


	}
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

				// Try f v/vt/vn
				int matched = sscanf(lineStart,
					"f %d/%*d/%*d %d/%*d/%*d %d/%*d/%*d",
					&a, &b, &c);

				// Try f v
				if (matched != 3) {
					matched = sscanf(lineStart,
						"f %d %d %d",
						&a, &b, &c);
				}

				if (matched == 3) {
					face.a = a - 1;
					face.b = b - 1;
					face.c = c - 1;

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
		VirtualFree(buffer.depth, 0, MEM_RELEASE);
	}
	buffer.width = width;
	buffer.height = height;

	buffer.bmInfo.bmiHeader.biSize = sizeof(BITMAPINFO);
	buffer.bmInfo.bmiHeader.biWidth = width;
	buffer.bmInfo.bmiHeader.biHeight = -height; // previously we flipped it, because by default bitmap format 0,0 is the bottom LEFT for some reason? we want top right.
	buffer.bmInfo.bmiHeader.biPlanes = 1;
	buffer.bmInfo.bmiHeader.biBitCount = 32; // 8 bits padding, rgb is 24
	buffer.bmInfo.bmiHeader.biCompression = BI_RGB;


	u32 bytesPerPixel = 4;
	u32 size = width * height * bytesPerPixel;
	u32 depthSize = width * height * sizeof(float);

	buffer.memory = VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	buffer.depth = VirtualAlloc(nullptr, depthSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
	float* db = (float*)buffer.depth;
	//for (int i = 0; i < buffer.width * buffer.height; i++) {
	//	db[i] = FLT_MAX;
	//	
	//}
	std::fill(db, db + buffer.width * buffer.height, FLT_MAX);
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
			//PutPixel(x, y, 0x0);
		}
	}
}

void ClearFrameData(u32 color) {
	// for each pixel, set the 8 green bits on and turn everything else off
	// RR GG BB AA
	// AA BB GG RR
	// loop height->width because we go row -> each column entry -> next row?
	u32* pixels = (u32*)buffer.memory;
	float* db = (float*)buffer.depth;

	for (u32 y = 0; y < buffer.height; y++) {
		for (u32 x = 0; x < buffer.width; x++) {
			pixels[(y * buffer.width) + x] = color; // green
			db[(y * buffer.width) + x] = FLT_MAX;
			//PutPixel(x, y, 0x0);
		}
	}
}

//void ClearFrameData() {
//	float* db = (float*)buffer.depth;
//	//std::fill(db, db + buffer.width * buffer.height, FLT_MAX);
//	FramebufferRenderColour(0x00000000);
//}








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
		if (VKCode == 'R') {
			camera.rotation.x += 1;
			v = MakeViewMatrix();
		}
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
			//v = MakeViewMatrix();
			p = MakePerspective(30);
		}
		DisplayBitmapInWindow(winData, buffer);
		EndPaint(hwnd, &paint);
	}break;
	default: {
		//OutputDebugStringA("DefWindProc");
		result = DefWindowProc(hwnd, MSG, wParam, lParam); // h
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
		std::max(
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
			(u32)COLOURS::RED
		);

		x += xInc;
		y += yInc;
	}
}


struct Object {
	Model* model; // heavy ? use pointer.. can i even copy it... ? will perform a copy on the vec
	Transform transform;
	std::vector<Vector4> transformCache;
	std::vector<u32> cacheFrame;
	u32 frameCount;
};

constexpr float NEAR_PLANE = 1.f;
Vector4 RIGHT{ 1.f,0,0,0 };
Vector4 UP{ 0,1.f,0,0 };
Vector4 FORWARD{ 0,0,-1.f,0 };



// call this when the fov/aspect/camera pos or rot changes
Matrix4D MakeViewMatrix() {
	Matrix4D rot = Matrix4D::Rz(camera.rotation.z) * Matrix4D::Ry(camera.rotation.y) * Matrix4D::Rx(camera.rotation.x);
	Matrix4D rotInv = rot.Transpose();
	Vector4 translation = (rotInv * (-camera.position));
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
	Vector4 v_world = world * v;
	return v_world;
}

Matrix4D MakePerspective(float fovDeg) {
	float fov = degrees_to_radians(fovDeg);
	float tanHalfFovy = std::tanf(fov / 2);
	// f = fov scaling factor. we mul our x and y by this f to either shrink or grow objects depending on the fov(zoom factor)
	float f = 1 / tanHalfFovy;
	float& d = f;
	float ar = (float)buffer.height / (float)buffer.width;
	float zNear = 1.f;
	float zFar = 1000.f;
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
Vector2 NDCToScreen(Vector4 v) {
	float screenX1 = ((v.x + 1.0f) * 0.5f * buffer.width);
	float screenY1 = ((1.0f - v.y) * 0.5f * buffer.height);
	return { screenX1, screenY1 };
}
void ScanlineRasterize(Vector2 screen0, Vector2 screen1, Vector2 screen2) {
	// populate a triangle and draw it
	Triangle tri{ screen0,screen1,screen2 };

	Vector2 v[3] = { screen0,screen1,screen2 };
	std::sort(v, v + 3, [](const Vector2& a, const Vector2& b) {
		return a.y < b.y;
		});
	auto vtop = v[0];
	auto vmid = v[1];
	auto vbot = v[2];

	// edge slopes
	auto edge1 = (vmid.x - vtop.x) / (vmid.y - vtop.y);
	auto edge2 = (vmid.x - vbot.x) / (vmid.y - vbot.y);
	auto edge3 = (vtop.x - vbot.x) / (vtop.y - vbot.y);

	//lerp

	float t = (vmid.y - vtop.y) / (vbot.y - vtop.y); // find how far down along vtop->vbot the middle/vmid.y lies.
	Vector2 vsplit; //vsplit = where the middle vertex.y is, the row where the tri changes shape.
	vsplit.x = vtop.x + t * (vbot.x - vtop.x); // find the matching x based on t
	vsplit.y = vmid.y; // its on the same row as vmid.y so its the same y


	float invL = (vmid.x - vtop.x) / (vmid.y - vtop.y);
	float invR = (vsplit.x - vtop.x) / (vsplit.y - vtop.y);
	for (int y = ceilf(vtop.y); y <= vmid.y; y++) {

		float xL = vtop.x + (y - vtop.y) * invL;
		float xR = vtop.x + (y - vtop.y) * invR;

		if (xL > xR) std::swap(xL, xR);

		for (int x = (int)ceilf(xL); x < (int)ceilf(xR) - 1; x++) {
			PutPixel(x, y, (u32)COLOURS::SKIN);
		}
	}

	invL = (vbot.x - vmid.x) / (vbot.y - vmid.y);
	invR = (vbot.x - vsplit.x) / (vbot.y - vsplit.y);

	for (int y = ceil(vmid.y); y <= vbot.y; y++) {

		float xL = vmid.x + (y - vmid.y) * invL;
		float xR = vsplit.x + (y - vmid.y) * invR;

		if (xL > xR) std::swap(xL, xR);

		for (int x = (int)ceilf(xL); x < (int)ceilf(xR) - 1; x++) {
			PutPixel(x, y, (u32)COLOURS::SKIN);
		}
	}
}


bool PointInTriangle(Vector2 p, Vector2 tri[3])
{
	Vector2 edgeA = tri[1] - tri[0]; // A->B (B-A = A->B)
	Vector2 edgeB = tri[2] - tri[1]; // B ->C
	Vector2 edgeC = tri[0] - tri[2]; // C -> A - WRAPS

	Vector2 ap = p - tri[0];
	Vector2 bp = p - tri[1];
	Vector2 cp = p - tri[2];

	float crossA = edgeA.x * ap.y - edgeA.y * ap.x;
	float crossB = edgeB.x * bp.y - edgeB.y * bp.x;
	float crossC = edgeC.x * cp.y - edgeC.y * cp.x;


	bool hasNeg = (crossA < 0) || (crossB < 0) || (crossC < 0);
	bool hasPos = (crossA > 0) || (crossB > 0) || (crossC > 0);

	return !(hasNeg && hasPos);
}




inline float Cross2D(const Vector2& lhs, const Vector2& rhs) {
	return lhs.x * rhs.y - lhs.y * rhs.x;
}
bool IsTopLeft(Vector2 edge) {
	bool goesUp = edge.y < 0.0f;
	bool isHorizontalTop = edge.y == 0.0f && edge.x > 0.0f;
	return goesUp || isHorizontalTop;
}
bool EdgeCheck(float edgeValue, bool isTopLeft) {
	return edgeValue > 0.0f || (IsNearlyZero(edgeValue) && isTopLeft);
}
bool PointInTriangle(Vector2 p, Triangle tri) {
	// area gives winding info. positive = CCW, 0 = degen, negative = CW
	//float area = Cross2D(tri.v1 - tri.v0, tri.v2 - tri.v0);

	//// degenrate tris
	//if (area == 0.0f) {
	//	return false;
	//}
	//// normalise winding to force CCW
	//if (area < 0.0f) {
	//	std::swap(tri.v1, tri.v2);
	//}

	// make edge vectors for positive/CCW winding
	// a -> b (b - a)
	Vector2 abEdge = tri.v1 - tri.v0;

	// b - > c(c- b)
	Vector2 bcEdge = tri.v2 - tri.v1;

	// c - > a (a - c)
	Vector2 caEdge = tri.v0 - tri.v2;


	// Top Left Rule - a pixel is considered in a triangle if it lies on a left edge, or a flat top edge.


	// make vectors pointing from edge tail(start) to point p

	Vector2 abToP = p - tri.v0;
	Vector2 bcToP = p - tri.v1;
	Vector2 caToP = p - tri.v2;


	// compute cross of each


	float crossAB = Cross2D(abEdge, abToP);
	float crossBC = Cross2D(bcEdge, bcToP);
	float crossCA = Cross2D(caEdge, caToP);




	// CCW triangle: inside is left of every edge.
	return EdgeCheck(crossAB, IsTopLeft(abEdge)) &&
		EdgeCheck(crossBC, IsTopLeft(bcEdge)) &&
		EdgeCheck(crossCA, IsTopLeft(caEdge));

}

#include <format> 
struct Colour {
	u8 r;
	u8 g;
	u8 b;
};

Colour colours[3] = { {0xFF,0x00,0x00},{0x00,0xFF,0x00},{0x00,0x00,0xFF} };

static float oldcrossAB;
int tick = 0;

#include <expected>

 std::expected<std::vector<std::byte>,bool> LoadBMP(std::filesystem::path& path) {
	// load entire file into memory
	if (!std::filesystem::is_regular_file(path)) {
		return std::unexpected(false);
	}
	// open final in binary mode, seek to end instantly 2 get size
	std::ifstream file(path, std::ios::binary | std::ios::ate);
	if (!file.is_open()) {
		// handle
		return std::unexpected(false);
	}
	std::streamsize fileSize = file.tellg();
	if (fileSize < 0) {
		return std::unexpected(false);
	}
	file.seekg(0, std::ios::beg);
	
	std::vector<std::byte> bytes;
	bytes.resize(fileSize);
	
	if (!file.read((char*) bytes.data(), fileSize)) {
		// return failed
		return std::unexpected(false);
	}





	return bytes;




	
}

struct FileObject {
	// user must free data
	char* data;
	size_t size;
	void Free() {
		delete[] data;
	}
};
bool ReadFile(const std::filesystem::path& path, FileObject& fileObj) {
	// load entire file into memory
	if (!std::filesystem::is_regular_file(path)) {
		__debugbreak();
		return false;
	}
	// open final in binary mode, seek to end instantly
	std::ifstream file(path, std::ios::binary | std::ios::ate);
	// return to start
	if (!file.is_open()) {
		// handle
		return false;
	}
	std::streamsize fileSize = file.tellg();
	if (fileSize < 0) {
		return false;
	}
	file.seekg(0, std::ios::beg);
	char* output = new char[fileSize];
	if (!file.read(output, fileSize)) {
		// return failed
		delete[] output;
		return false;
	}
	fileObj.data = output;
	fileObj.size = fileSize;
	return true;
}
struct RenderVertex {
	Vector2 pos;
	Vector2 uv;
	float depth;
	float invW;
};
u32 SampleTexture(float u, float v, Texture& texture) {
	u = std::clamp(u, 0.0f, 1.0f);
	v = std::clamp(v, 0.0f, 1.0f);
	int x = int(u * (texture.width - 1));
	int y = int(v * (texture.height - 1));
	return texture.data[y * texture.width + x];
}
void EdgeFunctionRasterize(RenderVertex& v0, RenderVertex& v1, RenderVertex& v2, Texture& texture) {

	
	Vector2 v0pos = v0.pos;
	Vector2 v1pos = v1.pos;
	Vector2 v2pos = v2.pos;
	Triangle tri = { v0pos,v1pos,v2pos };

	// compute bounding box
	auto [trixmin, trixmax] = std::minmax({ v0pos.x,v1pos.x,v2pos.x });
	auto [triymin, triymax] = std::minmax({ v0pos.y,v1pos.y,v2pos.y });

	// inital rejection if off screen.. i think it has been culled by here anyway..
	if (trixmax < 0 || trixmin >= buffer.width ||
		triymax < 0 || triymin >= buffer.height)
	{
		return;
	}

	// clamp to screen dimensions
	auto xmin = std::clamp((int)floor(trixmin), 0, buffer.width - 1);
	auto xmax = std::clamp((int)ceil(trixmax), 0, buffer.width - 1);
	auto ymin = std::clamp((int)floor(triymin), 0, buffer.height - 1);
	auto ymax = std::clamp((int)ceil(triymax), 0, buffer.height - 1);



	// 2D cross of a tri = 2xSignedArea of tri, so it can tell you about the triangle
	// > 0, its a CCW tri, == 0 its a degen tri, < 0 its CW
	float area = Cross2D(tri.v1 - tri.v0, tri.v2 - tri.v0);

	// degenrate tris
	if (IsNearlyZero(area)) {
		return;
	}
	
	// normalise winding to force CCW
	if (area < 0.0f) {
		std::swap(v1,v2);
		area = Cross2D(tri.v1 - tri.v0, tri.v2 - tri.v0); //recompute area
	}
	tri = { v0.pos,v1.pos,v2.pos };

	// a - > b (b-a)
	Vector2 abEdge = tri.v1 - tri.v0;
	// b - > c(c- b)
	Vector2 bcEdge = tri.v2 - tri.v1;
	// c - > a (a - c)
	Vector2 caEdge = tri.v0 - tri.v2;


	// w0 = bcEdge
	float deltaBcCol = tri.v1.y - tri.v2.y;  // -(c.y - b.y)
	float deltaBcRow = tri.v2.x - tri.v1.x;  //  c.x - b.x

	// w1 = caEdge
	float deltaCaCol = tri.v2.y - tri.v0.y;  // -(a.y - c.y)
	float deltaCaRow = tri.v0.x - tri.v2.x;  //  a.x - c.x

	// w2 = abEdge
	float deltaAbCol = tri.v0.y - tri.v1.y;  // -(b.y - a.y)
	float deltaAbRow = tri.v1.x - tri.v0.x;  //  b.x - a.x


	Vector2 point0 = { float(xmin + 0.5f), float(ymin + 0.5f) };

	float abCrossRow0 = Cross2D(abEdge, point0 - tri.v0);
	float bcCrossRow0 = Cross2D(bcEdge, point0 - tri.v1);
	float caCrossRow0 = Cross2D(caEdge, point0 - tri.v2);

	bool abFlag = IsTopLeft(abEdge);
	bool bcFlag = IsTopLeft(bcEdge);
	bool caFlag = IsTopLeft(caEdge);
	float invArea = 1.0f / area;
	for (int y = ymin; y <= ymax; y++) {

		float crossAB = abCrossRow0;
		float crossBC = bcCrossRow0;
		float crossCA = caCrossRow0;

		for (int x = xmin; x <= xmax; x++) {



			// go to pixel centre
			//Vector2 p{ (float)x + 0.5f, (float)y + 0.5f };

			//Vector2 abToP = p - tri.v0;
			//Vector2 bcToP = p - tri.v1;
			//Vector2 caToP = p - tri.v2;

			//// compute cross of each
			//float crossAB = Cross2D(abEdge, abToP);
			//float crossBC = Cross2D(bcEdge, bcToP);
			//float crossCA = Cross2D(caEdge, caToP);



			if (EdgeCheck(crossAB, abFlag) &&
				EdgeCheck(crossBC, bcFlag) &&
				EdgeCheck(crossCA, caFlag))
			{
				//barycentrics 

				float alpha = crossBC * invArea;
				float beta = crossCA * invArea;
				float gamma = crossAB * invArea;



				float* d = (float*)buffer.depth;
				float oldDepth = d[y * buffer.width + x];


				float newDepth = alpha * v0.depth + beta * v1.depth + gamma * v2.depth; // interpolate depth using barycentrics

				if ((newDepth >= oldDepth)) {
					continue;
				}

				d[y * buffer.width + x] = newDepth;





				//u8 a = 0xFF;
				//u8 r = alpha * colours[0].r + beta * colours[1].r + gamma * colours[2].r;
				//u8 g = alpha * colours[0].g + beta * colours[1].g + gamma * colours[2].g;
				//u8 b = alpha * colours[0].b + beta * colours[1].b + gamma * colours[2].b;





				// old
				//float u = alpha * v0.uv.u + beta * v1.uv.u + gamma * v2.uv.u;
				//float v = alpha * v0.uv.v + beta * v1.uv.v + gamma * v2.uv.v;

				// perspective correct
				float denominator = (alpha * v0.invW) + (beta * v1.invW) + (gamma * v2.invW);
				float u = ((alpha * v0.uv.u * v0.invW) + (beta * v1.uv.u * v1.invW) + (gamma * v2.uv.u * v2.invW)) / denominator;
				float v = ((alpha * v0.uv.v * v0.invW) + (beta * v1.uv.v * v1.invW) + (gamma * v2.uv.v * v2.invW)) / denominator;
		


				u32 rgba = SampleTexture(u, v, texture);

				uint8_t r = rgba & 0xFF;
				uint8_t g = (rgba >> 8) & 0xFF;
				uint8_t b = (rgba >> 16) & 0xFF;
				uint8_t a = (rgba >> 24) & 0xFF;

				uint32_t bgra =
					(a << 24) |
					(r << 16) |  // goes into byte 2
					(g << 8) |
					b;           // goes into byte 0

				



				//float u = alpha * uvA.u + beta * uvB.u + gamma * uvC.u;
				//float v = alpha * uvA.v + beta * uvB.v + gamma * uvC.v;


				u32 colour = 0x00000000;
				//colour = (colour | a) << 8;
				colour = (colour | r) << 8;
				colour = (colour | g) << 8;
				colour = (colour | b);

				PutPixel(x, y, bgra);
			}
			// acculumuate across the x
			crossAB += deltaAbCol;
			crossBC += deltaBcCol;
			crossCA += deltaCaCol;
		}
		// acculumuate across the y
		abCrossRow0 += deltaAbRow;
		bcCrossRow0 += deltaBcRow;
		caCrossRow0 += deltaCaRow;
	}
}
volatile int hits = 0;
volatile int misses = 0;


// Sutherland–Hodgman polygon clippppper
Vertex IntersectNear(Vertex a, Vertex b) {
	float t = (-NEAR_PLANE - a.pos.z) / (b.pos.z - a.pos.z);

	// clip pos
	Vector4 r;
	r.x = Lerp(a.pos.x, b.pos.x, t);
	r.y = Lerp(a.pos.y, b.pos.y, t);
	r.z = Lerp(a.pos.z, b.pos.z, t);

	// clip uv
	Vector2 uv;
	uv.u = Lerp(a.uv.u, b.uv.u, t);
	uv.v = Lerp(a.uv.v, b.uv.v, t);

	Vector4 norm;
	norm.x = Lerp(a.normal.x, b.normal.x, t);
	norm.y = Lerp(a.normal.y, b.normal.y, t);
	norm.z = Lerp(a.normal.z, b.normal.z, t);

	Vertex ret;
	ret.colour = a.colour;
	ret.normal = norm;
	ret.pos = r;
	ret.uv = uv;
	return ret;
}
bool InsideNear(Vector4 v) {
	return v.z <= -NEAR_PLANE;
}
std::vector<Vertex> ClipAgainstNear(const std::vector<Vertex>& input) {

	std::vector<Vertex> out;
	for (int i = 0; i < input.size(); ++i) {
		Vertex a = input[i];
		Vertex b = input[(i + input.size() - 1) % input.size()];

		Vector4 cur = a.pos;
		Vector4 prev = b.pos;

		bool currInside = InsideNear(cur);
		bool prevInside = InsideNear(prev);

		// case 1 - both inside
		if (currInside && prevInside) {
			out.push_back(a);
		}
		// case 2  inside->outside - keep the edge up to the plane(intersection)
		else if (prevInside && !currInside) {
			out.push_back(IntersectNear(b, a));
		}
		// case 3 outside -> inside

		else if (!prevInside && currInside) {
			out.push_back(IntersectNear(b, a));
			out.push_back(a);
		}
		// case 4 - both outside, return insertsect of prev and current unchanged
	}
	return out;

}

// sep function so clipping that produces a new tri works

void HandleViewSpaceTriangle(const Vertex& v0view,
	const Vertex& v1view,
	const Vertex& v2view, Texture& texture) {

	auto v0clip = p * v0view.pos;
	auto v1clip = p * v1view.pos;
	auto v2clip = p * v2view.pos;

	float w0 = v0clip.w;
	float w1 = v1clip.w;
	float w2 = v2clip.w;

	v0clip /= w0;
	v1clip /= w1;
	v2clip /= w2;


	auto screen0 = NDCToScreen(v0clip);
	auto screen1 = NDCToScreen(v1clip);
	auto screen2 = NDCToScreen(v2clip);


	// Prepare RenderVerts - TODO - Stop so much copying..
	RenderVertex rv0, rv1, rv2;
	rv0.pos = screen0;
	rv0.uv = v0view.uv;
	rv0.depth = v0clip.z;
	rv0.invW = 1.0f / w0;

	rv1.pos = screen1;
	rv1.uv = v1view.uv;
	rv1.depth = v1clip.z;
	rv1.invW = 1.0f / w1;

	rv2.pos = screen2;
	rv2.uv = v2view.uv;
	rv2.depth = v2clip.z;
	rv2.invW = 1.0f / w2;
	

	//if (v0clip.z < -1 || v0clip.z > 1 || v1clip.z < -1 || v1clip.z > 1 || v2clip.z < -1 || v2clip.z > 1) {
	//	return;
	//}

	EdgeFunctionRasterize(rv0,rv1,rv2, texture);
}
void RenderModels(std::vector<Object>& models) {
	//models[0].transformCache.assign(models[0].model->verts.size(), Vector4{ 0,0,0,0 }); // TODO -temp fix, 
	// instead use a cache flag boolean array, then instead if if(isEmpty), do if(flag), if not then recacl
	for (auto& m : models) {
		auto& faces = m.model->faces;
		Transform transform = m.transform;
		Matrix4D model = MakeModelMatrix(transform);
		//Matrix4D MVP = p * v * model; // 
		Matrix4D MV = v * model;

		float xmin = FLT_MAX;
		float ymin = FLT_MAX;
		float xmax = -FLT_MAX;
		float ymax = -FLT_MAX;

		for (int i = 0; i < m.model->indexBuffer.size(); i += 3) {

			//Vector4 v0{ m.model->verts[face.a],1 }; // THESE ARE IN LOCAL SPACE
			//Vector4 v1{ m.model->verts[face.b],1 };
			//Vector4 v2{ m.model->verts[face.c],1 };

			u32 idx0 = m.model->indexBuffer[i];
			u32 idx1 = m.model->indexBuffer[i + 1];
			u32 idx2 = m.model->indexBuffer[i + 2];

			auto vertex0 = m.model->vertices[idx0];
			auto vertex1 = m.model->vertices[idx1];
			auto vertex2 = m.model->vertices[idx2];



			Vector4 v0pos = vertex0.pos;
			Vector4 v1pos = vertex1.pos;
			Vector4 v2pos = vertex2.pos;

			

			Vector4 v0view;
			Vector4 v1view;
			Vector4 v2view;
			v0view = MV * v0pos;
			v1view = MV * v1pos;
			v2view = MV * v2pos;


			// transform caching
			/*if (m.cacheFrame[idx0] == m.frameCount) {
				v0view = m.transformCache[idx0];
				hits++;
			}
			else {
				v0view = MV * v0;
				m.transformCache[idx0] = v0view;
				m.cacheFrame[idx0] = m.frameCount;
				misses++;
			}
			if (m.cacheFrame[idx1] == m.frameCount) {
				v1view = m.transformCache[idx1];
				hits++;
			}
			else {
				v1view = MV * v1;
				m.transformCache[idx1] = v1view;
				m.cacheFrame[idx1] = m.frameCount;
				misses++;
			}
			if (m.cacheFrame[idx2] == m.frameCount) {
				v2view = m.transformCache[idx2];
				hits++;
			}
			else {
				v2view = MV * v2;
				m.transformCache[idx2] = v2view;
				m.cacheFrame[idx2] = m.frameCount;
				misses++;
			}*/

			volatile int sink = hits;
			sink = misses;
			// backface culling



			// calculate surface normal
			Vector4 edge1 = (v1view - v0view);
			Vector4 edge2 = (v2view - v0view);
			Vector4 normal = edge1.cross(edge2).normalized();


			// calculate a vector pointing at the camera/screen/eye
			Vector4 triToEye = -v0view;

			// compare with vector that we know is pointing at the camera. >0 = same relative dir. else cull
			if (normal.Dot(triToEye) <= 0) {
				continue;
			}



			//auto v0world = model * v0;
			//auto v1world = model * v1;
			//auto v2world = model * v2;

			//Vector4 v0view = v * v0world;
			//Vector4 v1view = v * v1world;
			//Vector4 v2view = v * v2world;



			// NEAR PLANE CLIPPER

			vertex0.pos = v0view;
			vertex1.pos = v1view;
			vertex2.pos = v2view;
			std::vector<Vertex> clipped = ClipAgainstNear({ vertex0,vertex1,vertex2 });

			// < 3 because 2 verts cant form a tri, 3 and 4 can..
			if (clipped.size() < 3) {
				continue;
			}


		

			for (int i = 1; i + 1 < clipped.size(); ++i) {
				vertex0 = clipped[0];
				vertex1 = clipped[i];
				vertex2 = clipped[i + 1];;
				HandleViewSpaceTriangle(vertex0,vertex1,vertex2,m.model->texture);
			}

			//TODO - MAKE BETTER CLIPPING. this is crude. BVH
			//xmin = std::min({ xmin, screen0.x, screen1.x, screen2.x });
			//xmax = std::max({ xmax, screen0.x, screen1.x, screen2.x });

			//ymin = std::min({ ymin, screen0.y, screen1.y, screen2.y });
			//ymax = std::max({ ymax, screen0.y, screen1.y, screen2.y });
		}


		// debug box - UNSAFE. add buffer.w/h checking
		//int left = (int)floorf(xmin);
		//int right = (int)ceilf(xmax);
		//int top = (int)floorf(ymin);
		//int bottom = (int)ceilf(ymax);

		//for (int x = left; x <= right; x++)
		//{
		//	PutPixel(x, top, (u32)COLOURS::RED);
		//	PutPixel(x, bottom, (u32)COLOURS::RED);
		//}

		//for (int y = top; y <= bottom; y++)
		//{
		//	PutPixel(left, y, (u32)COLOURS::RED);
		//	PutPixel(right, y, (u32)COLOURS::RED);
		//}
	}
}




void DrawTriangle(Triangle t) {
	if (t.v0.x == INFINITY || t.v1.x == INFINITY || t.v2.x == INFINITY) {
		return;
	}
	DrawLine(
		(i32)t.v0.x,
		(i32)t.v0.y,
		(i32)t.v1.x,
		(i32)t.v1.y
	);

	DrawLine(
		(i32)t.v1.x,
		(i32)t.v1.y,
		(i32)t.v2.x,
		(i32)t.v2.y
	);

	DrawLine(
		(i32)t.v2.x,
		(i32)t.v2.y,
		(i32)t.v0.x,
		(i32)t.v0.y
	);
}


void PutPixel(i32 x, i32 y, u32 color)
{
	if (x < 0 || y < 0 || x >= buffer.width || y >= buffer.height) { // think im covering this elsewhere.
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

	AllocConsole();
	freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);


	int x, y, n;
	unsigned char* tex = stbi_load("white.jpg", &x, &y, &n, 4);
	Texture texture{ x,y,reinterpret_cast<u32*>(tex)};

	std::filesystem::path path("diablo3.obj");
	Model model(path);
	model.texture = texture;
	
	Object object;
	object.model = &model;
	
	std::vector<Vector4> tcache(object.model->vertices.size(), 0);
	std::vector<u32> cacheFrame(object.model->vertices.size(), 0);
	object.transformCache = tcache;
	object.cacheFrame = cacheFrame;


	Transform transform;
	transform.position = { 0,0,0,1 };
	transform.scale = { 1.f };
	transform.rotation = { 0,0,0,0 };
	object.transform = transform;

	std::vector<Object> objects;
	objects.push_back(object);



	camera.position = Vector4{ 0, 0, 5, 1 };
	camera.rotation = Vector4{ 0, 0, 0, 0 };
	v = MakeViewMatrix();
	p = MakePerspective(30);


	WNDCLASSEX windowclass{};
	windowclass.style = CS_HREDRAW | CS_VREDRAW;
	windowclass.hInstance = Instance;
	windowclass.lpszClassName = L"WindowClass";
	windowclass.lpfnWndProc = WndProc;
	windowclass.cbSize = sizeof(WNDCLASSEX);
	HRESULT windowResult = RegisterClassEx(&windowclass);
	if (!windowResult) { OutputDebugStringA("Failed to register window class"); return 0; }
	HWND hwnd = CreateWindowExW(0, windowclass.lpszClassName, L"Software Rasterizier",
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

	Model cube;

	cube.positions = {
		{-1, -1, -1, 1}, // 0
		{ 1, -1, -1, 1}, // 1
		{ 1,  1, -1, 1}, // 2
		{-1,  1, -1, 1}, // 3
		{-1, -1,  1, 1}, // 4
		{ 1, -1,  1, 1}, // 5
		{ 1,  1,  1, 1}, // 6
		{-1,  1,  1, 1}, // 7
	};

	cube.uvs = {
		{0,0},
		{1,0},
		{1,1},
		{0,1}
	};

	cube.normals = {
		{ 0,  0,  1}, // front
		{ 0,  0, -1}, // back
		{-1,  0,  0}, // left
		{ 1,  0,  0}, // right
		{ 0,  1,  0}, // top
		{ 0, -1,  0}  // bottom
	};


	// Helper lambda to create vertices exactly like ReadObj()
	auto addVertex = [&](int pos, int uv, int normal)
		{
			Vertex v;
			v.pos = cube.positions[pos];
			v.uv = cube.uvs[uv];
			v.normal = cube.normals[normal];

			cube.vertices.push_back(v);
			cube.indexBuffer.push_back((u32)cube.vertices.size() - 1);
		};


	// Front (+Z)
	addVertex(4, 0, 0);
	addVertex(5, 1, 0);
	addVertex(6, 2, 0);

	addVertex(4, 0, 0);
	addVertex(6, 2, 0);
	addVertex(7, 3, 0);


	// Back (-Z)
	addVertex(1, 0, 1);
	addVertex(0, 1, 1);
	addVertex(3, 2, 1);

	addVertex(1, 0, 1);
	addVertex(3, 2, 1);
	addVertex(2, 3, 1);


	// Left (-X)
	addVertex(0, 0, 2);
	addVertex(4, 1, 2);
	addVertex(7, 2, 2);

	addVertex(0, 0, 2);
	addVertex(7, 2, 2);
	addVertex(3, 3, 2);


	// Right (+X)
	addVertex(5, 0, 3);
	addVertex(1, 1, 3);
	addVertex(2, 2, 3);

	addVertex(5, 0, 3);
	addVertex(2, 2, 3);
	addVertex(6, 3, 3);


	// Top (+Y)
	addVertex(3, 0, 4);
	addVertex(7, 1, 4);
	addVertex(6, 2, 4);

	addVertex(3, 0, 4);
	addVertex(6, 2, 4);
	addVertex(2, 3, 4);


	// Bottom (-Y)
	addVertex(0, 0, 5);
	addVertex(1, 1, 5);
	addVertex(5, 2, 5);

	addVertex(0, 0, 5);
	addVertex(5, 2, 5);
	addVertex(4, 3, 5);

	

	cube.texture = texture;
	Object cubeObj;
	cubeObj.model = &cube;
	transform.position = { 0,0,0,1 };
	cubeObj.transform = transform;
	std::vector<Object> cub = { cubeObj };

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

		// rotate object around
		cub[0].transform.rotation.y += 0.1;
		//cub[0].transform.rotation.z += 0.01;

		objects[0].frameCount++;
		ClearFrameData(0x0);
		RenderModels(cub);
		render(hwnd);

		// perf
		LARGE_INTEGER endCounter;
		QueryPerformanceCounter(&endCounter);
		i64 endCycleCount = __rdtsc();
		i64 totalCycles = endCycleCount - lastCycleCount;
		double frameTime = ((double)(endCounter.QuadPart - lastCounter.QuadPart) / (double)frequency.QuadPart);
		float FPS = 1.0f / frameTime;
		char buf[256];
		sprintf_s(buf, 256, "FPS : %.3f  \n Mega Cycles taken for this frame: %d \n ", FPS, totalCycles / (1000 * 1000));
		//std::cout << "fps : " << FPS << std::endl << "\n Mega Cycles taken for this frame: " << totalCycles / (1000 * 1000);
		OutputDebugStringA(buf);
		lastCycleCount = endCycleCount;
		lastCounter = endCounter;

	}
	stbi_image_free(tex);
	return 1;
}


