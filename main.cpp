//+X = right
//+ Y = up
//- Z = forward

#define NOMINMAX // GO AWAY WINDOWS
#define _CRT_SECURE_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN


#include "math.h"
#include <Windows.h>
#include <cstdint>
#include <iostream>


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


Matrix4D MakePerspective();
Matrix4D MakeViewMatrix();
void DrawLine(u32 x0, u32 y0, u32 x1, u32 y1);
inline void render(HWND hwnd);
void DrawTriangle(Triangle t);



struct Model {
	std::vector<Vector3> verts;
	std::vector<Face> faces;


	Model(std::vector<Vector3> verts, std::vector<Face> faces) : verts(verts), faces(faces) {};

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

void ClearFrameData() {
	float* db = (float*)buffer.depth;
	std::fill(db, db + buffer.width * buffer.height, FLT_MAX);
	FramebufferRenderColour(0x00000000);
}
	







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
			camera.rotation.y += 1;
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
			p = MakePerspective();
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

Matrix4D MakePerspective() {
	float fov = degrees_to_radians(45.6);
	float tanHalfFovy = std::tanf(fov / 2);
	// f = fov scaling factor. we mul our x and y by this f to either shrink or grow objects depending on the fov(zoom factor)
	float f = 1 / tanHalfFovy;
	float& d = f;
	float ar = (float)buffer.height / (float)buffer.width;
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

		for (int x = (int)ceilf(xL); x < (int)ceilf(xR)-1; x++) {
			PutPixel(x, y, (u32)COLOURS::SKIN);
		}
	}

	invL = (vbot.x - vmid.x) / (vbot.y - vmid.y);
	invR = (vbot.x - vsplit.x) / (vbot.y - vsplit.y);

	for (int y = ceil(vmid.y); y <= vbot.y; y++) {

		float xL = vmid.x + (y - vmid.y) * invL;
		float xR = vsplit.x + (y - vmid.y) * invR;

		if (xL > xR) std::swap(xL, xR);

		for (int x = (int)ceilf(xL); x < (int)ceilf(xR) -1; x++) {
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
void EdgeFunctionRasterize(Vector2 v0, Vector2 v1, Vector2 v2, float depth[3]) {

	Triangle tri = { v0,v1,v2 };

	// compute bounding box
	auto [trixmin, trixmax] = std::minmax({ v0.x,v1.x,v2.x });
	auto [triymin, triymax] = std::minmax({ v0.y,v1.y,v2.y });

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
		std::swap(tri.v1, tri.v2);
		std::swap(depth[1], depth[2]);
		area = Cross2D(tri.v1 - tri.v0, tri.v2 - tri.v0); //recompute area
	}

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

	// if covered pixel is tiny toss it... idk about this, zoom out = less pixels..looks weird
	//if (area < 1.0f) {
	//	return;
	//}

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

				float alpha = crossBC* invArea;
				float beta =  crossCA* invArea;
				float gamma = crossAB* invArea;

				

				float* d = (float*)buffer.depth;
				float oldDepth = d[y * buffer.width + x];

				
				float newDepth = alpha * depth[0] + beta * depth[1] + gamma * depth[2]; // interpolate depth using barycentrics

				if (!(newDepth < oldDepth)) {
					continue;
				}

				d[y * buffer.width + x] = newDepth;



				u8 a = 0xFF;
				u8 r = alpha * colours[0].r + beta * colours[1].r + gamma * colours[2].r;
				u8 g = alpha * colours[0].g + beta * colours[1].g + gamma * colours[2].g;
				u8 b = alpha * colours[0].b + beta * colours[1].b + gamma * colours[2].b;

				u32 colour = 0x00000000;
				colour = (colour | a) << 8;
				colour = (colour | b) << 8;
				colour = (colour | g) << 8;
				colour = (colour | r);

				PutPixel(x, y, colour);
			}
			// acculumuate across the x
			crossAB += deltaAbCol;
			crossBC += deltaBcCol;
			crossCA += deltaCaCol;
		}
		// acculumuate across the y
		abCrossRow0 += deltaAbRow;
		bcCrossRow0	+= deltaBcRow;
		caCrossRow0	+= deltaCaRow;
	}
}

void RenderModels(std::vector<Object>& models) {
	models[0].transformCache.assign(models[0].model->verts.size(), Vector4{ 0,0,0,0 }); // TODO: This is a temp fix, 
	// instead use a cache flag boolean array, then instead if if(isEmpty), do if(flag), if not then recacl
	for (auto& m : models) {
		auto& faces = m.model->faces;
		Transform transform = m.transform;
		Matrix4D model = MakeModelMatrix(transform);
		Matrix4D MVP = p * v * model;
		Matrix4D MV = v * model;
		float xmin= FLT_MAX;
		float ymin= FLT_MAX;
		float xmax= -FLT_MAX;
		float ymax= -FLT_MAX;
		for (const auto& face : faces) {

			Vector4 v0{ m.model->verts[face.a],1 }; // THESE ARE IN LOCAL SPACE
			Vector4 v1{ m.model->verts[face.b],1 };
			Vector4 v2{ m.model->verts[face.c],1 };



			Vector4 v0view;
			Vector4 v1view;
			Vector4 v2view;

			if (!m.transformCache[face.a].isEmpty()) {
				v0view = m.transformCache[face.a];
			}
			else {
				Vector4 v0world = model * v0;
				v0view = v * v0world;
				m.transformCache[face.a] = v0view;
			}
			if (!m.transformCache[face.b].isEmpty()) {
				v1view = m.transformCache[face.b];
			}
			else {
				Vector4 v1world = model * v1;
				v1view = v * v1world;
				m.transformCache[face.b] = v1view;
			}
			if (!m.transformCache[face.c].isEmpty()) {
				v2view = m.transformCache[face.c];
			}
			else {
				Vector4 v2world = model * v2;
				v2view = v * v2world;
				m.transformCache[face.c] = v2view;
			}

			//auto v0world = model * v0;
			//auto v1world = model * v1;
			//auto v2world = model * v2;

			//Vector4 v0view = v * v0world;
			//Vector4 v1view = v * v1world;
			//Vector4 v2view = v * v2world;




			m.transformCache[face.a] = v0view;
			m.transformCache[face.b] = v1view;
			m.transformCache[face.c] = v2view;


			// TODO - BACKFACE CULLING IN VIEW SPACE
			// compute normal and facing, take 3d cross. if < 0, cull



			if (v0view.z <= -NEAR_PLANE &&
				v1view.z <= -NEAR_PLANE &&
				v2view.z <= -NEAR_PLANE) {

				auto v0clip = p * v0view;
				auto v1clip = p * v1view;
				auto v2clip = p * v2view;





				v0clip /= v0clip.w;
				v1clip /= v1clip.w;
				v2clip /= v2clip.w;



				auto screen0 = NDCToScreen(v0clip);
				auto screen1 = NDCToScreen(v1clip);
				auto screen2 = NDCToScreen(v2clip);

				 //depth buffer is [-1,1]. smaller the z the closer the camera

				if (v0clip.z < -1 || v0clip.z > 1 || v1clip.z < -1 || v1clip.z > 1 || v2clip.z < -1 || v2clip.z > 1) {
					continue;
				}

				float* d = new float[3];
				d[0] = v0clip.z; d[1] = v1clip.z; d[2] = v2clip.z;
				//ScanlineRasterize(screen0, screen1, screen2);
				EdgeFunctionRasterize(screen0, screen1, screen2,d);

				delete[] d;

				//DrawTriangle(tri);
		
				xmin = std::min({ xmin, screen0.x, screen1.x, screen2.x });
				xmax = std::max({ xmax, screen0.x, screen1.x, screen2.x });

				ymin = std::min({ ymin, screen0.y, screen1.y, screen2.y });
				ymax = std::max({ ymax, screen0.y, screen1.y, screen2.y });
		}
		}

		//// debug box
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
	if (x < 0 || y < 0 || x >= buffer.width || y >= buffer.height) {
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




#include <span>
int WINAPI wWinMain(HINSTANCE Instance, HINSTANCE PrevInstance, LPWSTR lpCmdLine, int CmdShow) {


	Model model("Mutsuki.obj");
	//Model model2b("2B.obj");

	Object object;
	object.model = &model;

	std::vector<Vector4> tcache(object.model->verts.size(), 0);

	object.transformCache = tcache;
	

	Object object2;
	//object2.model = &model2b;

	Transform transform;
	transform.position = { 0,0,0,1 };
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


	camera.position = Vector4{ 0, 0, 5, 1 };
	camera.rotation = Vector4{ 0, 0, 0, 0 };

	v = MakeViewMatrix();
	p = MakePerspective();


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


	i64 lastCycleCount = __rdtsc();
	LARGE_INTEGER frequency;
	QueryPerformanceFrequency(&frequency);
	LARGE_INTEGER lastCounter;
	QueryPerformanceCounter(&lastCounter);
	MSG msg{};



	Vector2 v0 = { 400,400 };
	Vector2 v1 = { 800,400 };
	Vector2 v2 = { 400,800 };
	Vector2 v3 = { 900,900 };
	Vector2 v4 = { 750,200 };

	
	Model cube = {
		// verts
		{
			{ 0,  1,  0}, // 0 top
			{-1, -1, -1}, // 1 back-left
			{ 1, -1, -1}, // 2 back-right
			{ 0, -1,  1}, // 3 front
		},

		// faces (CCW winding viewed from outside)
		{
			// Front
			{0, 3, 2},

			// Right
			{0, 2, 1},

			// Back
			{0, 1, 3},

			// Bottom
			{1, 2, 3},
		}
	};
	Object cubeObj;
	cubeObj.model = &cube;
	transform.position = { 0,0,0,1 };
	cubeObj.transform = transform;
	std::vector<Object> cub = { cubeObj };


	
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
		objects[0].transform.rotation.y += 0.1;
		
		ClearFrameData();
		RenderModels(objects);
		//RenderModels(cub);
		//EdgeFunctionRasterize(v0, v1, v2,COLOURS::RED);
		//EdgeFunctionRasterize(v3, v2, v1, COLOURS::SKIN);
		//EdgeFunctionRasterize(v4, v1, v0, COLOURS::WHITE);
		render(hwnd);

		
		
		/*
		* 
			HandleInput(scene.camera)
			Update(scene.camera)
			update func maybe just loops through a list of objects to update, switches on camera and handles it..?
			renderer.render(scene)

		*/

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


//struct Camera {
//	Transform transform;
//	Matrix4D v;
//	Matrix4D p;
//	float fovDeg;
//
//
//
//};
//struct Scene {
//	std::vector<Object> objects;
//	Camera camera;
//};
//static Scene scene;
//Camera camera;
//Transform camTrans;
//camTrans.rotation = { 0,0,0,0 };
//camTrans.position = { 0,0,5,0 };
//camera.v = MakeViewMatrix();
//camera.fovDeg = 45;
//camera.p = MakePerspective(camera.fovDeg);
//struct Renderer {
//
//	Framebuffer buffer;
//
//
//	void Render(HWND hwnd, Scene& scene) {
//		ClearFrameData();
//		RenderModels(scene.objects);
//		render(hwnd);
//	}
//
//	void Resize(HWND hwnd) {
//		WindowData frameWinData(hwnd);
//		if (frameWinData.windowSize.windowWidth != buffer.width ||
//			frameWinData.windowSize.windowHeight != buffer.height) {
//			ResizeBitmap(buffer, frameWinData.windowSize.windowWidth, frameWinData.windowSize.windowHeight);
//			centreScreenX = frameWinData.windowSize.windowWidth / 2;
//			centreScreenY = frameWinData.windowSize.windowHeight / 2;
//		}
//	}
//
//
//};
/*
OK lets try again.

aim for now, singleton Scene instance, so i can access the matrices in my current pipeline
this can change
so instantiate in global, populate in main



I want
Camera { p, v, fov }
Scene{
models,camera
}
Renderer{buffer, resize,  render functions}
*/