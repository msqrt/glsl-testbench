
#include "window.h"
#include "gl_helpers.h"
#include "math_helpers.h"
#include "math.hpp"
#include "text_renderer.h"
#include <dwrite_3.h>
#include <d2d1_3.h>
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "winmm.lib")

#include <vector>
#include <map>
#include <array>

const int screenw = 1600, screenh = 900;

#include <random>

#include <locale>
#include <codecvt>
extern WPARAM down[];
extern std::wstring text;
extern int downptr, hitptr;

bool keyDown(UINT vk_code);
bool keyHit(UINT vk_code);

template<typename T>
using basic_type = typename std::remove_cv_t<std::remove_reference_t<T>>;

template<typename T1, typename T2>
constexpr bool basically_same = std::is_same<basic_type<T1>, basic_type<T2>>::value;

struct dual {
	dual(){}
	dual(const float& value, const float& dualValue = .0f) : value(value), dualValue(dualValue) {}
	dual(const dual& other) : value(other.value), dualValue(other.dualValue) {}
	template<typename T> dual& operator=(T&& other) { const dual b = dual(other); value = b.value; dualValue = b.dualValue; return *this; }
	float value;
	float dualValue;
};

template<typename T1, typename T2>
using either_dual = std::enable_if_t < basically_same<T1, dual> || basically_same<T2, dual>, dual >;

template<typename T1, typename T2>
either_dual<T1, T2> operator*(T1&& aT, T2&& bT) {
	const dual a(aT), b(bT);
	return dual{ a.value*b.value, a.value*b.dualValue + b.value*a.dualValue };
}

template<typename T1, typename T2>
either_dual<T1, T2> operator/(T1&& aT, T2&& bT) {
	const dual a(aT), b(bT);
	return dual{ a.value/b.value, (a.dualValue*b.value - b.dualValue*a.value)/(b.value*b.value) };
}

template<typename T1, typename T2>
either_dual<T1, T2> operator+(T1&& aT, T2&& bT) {
	const dual a(aT), b(bT);
	return dual{ a.value + b.value, a.dualValue + b.dualValue };
}

dual operator-(dual&& a) {
	return dual(-a.value, -a.dualValue);
}

template<typename T1, typename T2>
either_dual<T1, T2> operator-(T1&& aT, T2&& bT) {
	return dual(aT) + (-dual(bT));
}

template<typename T> dual& operator+=(dual& a, T&& b) { return a = a + dual(b); }
template<typename T> dual& operator-=(dual& a, T&& b) { return a = a - dual(b); }
template<typename T> dual& operator*=(dual& a, T&& b) { return a = a * dual(b); }
template<typename T> dual& operator/=(dual& a, T&& b) { return a = a / dual(b); }

struct verbose {
	verbose() {}
	verbose(const float& value, std::string&& pname = "") : value(value) { if (pname.length() == 0) name = std::to_string(value); else name = pname; }
	verbose(const verbose& other) : value(other.value), name(other.name) {}
	template<typename T> verbose& operator=(T&& other) { const verbose b = verbose(other); value = b.value; name = b.name; return *this; }
	float value;
	std::string name;
};

template<typename T1, typename T2>
using either_verbose = std::enable_if_t < basically_same<T1, verbose> || basically_same<T2, verbose>, verbose >;

template<typename T1, typename T2>
either_verbose<T1,T2> operator*(T1&& aT, T2&& bT) {
	const verbose a(aT), b(bT);
	return verbose{ a.value*b.value, "(" + a.name + " * " + b.name + ")" };
}

template<typename T1, typename T2>
either_verbose<T1, T2> operator/(T1&& aT, T2&& bT) {
	const verbose a(aT), b(bT);
	return verbose{ a.value / b.value, "(" + a.name + " / " + b.name + ")" };
}

template<typename T1, typename T2>
either_verbose<T1, T2> operator+(T1&& aT, T2&& bT) {
	const verbose a(aT), b(bT);
	return verbose{ a.value + b.value, "(" + a.name + " + " + b.name + ")" };
}

verbose operator-(verbose&& a) {
	return verbose(-a.value, "(-" + a.name + ")");
}

template<typename T1, typename T2>
either_verbose<T1, T2> operator-(T1&& aT, T2&& bT) {
	return verbose(aT) + (-verbose(bT));
}

template<typename T> verbose& operator+=(verbose& a, T&& b) { return a = a + verbose(b); }
template<typename T> verbose& operator-=(verbose& a, T&& b) { return a = a - verbose(b); }
template<typename T> verbose& operator*=(verbose& a, T&& b) { return a = a * verbose(b); }
template<typename T> verbose& operator/=(verbose& a, T&& b) { return a = a / verbose(b); }

int main() {
	{
		dual a(.5, 1.f);
		a = 10.f + a;
		a *= .5f;
		dual b{ 2.0f, 1.0f };

		auto c = a * (b + .5f) * .5f;
		printf("%g, %g\n", c.value, c.dualValue);
	}
	{
		verbose a(.5, "a");
		a = 10.f + a;
		a *= .5f;
		verbose b{ 2.0f, "b" };
		auto c = a * (b + .5f) * .5f;
		printf("c = %s\n", c.name.c_str());
	}

	Window window(screenw, screenh, "", false);

	LARGE_INTEGER start, current, frequency;
	QueryPerformanceFrequency(&frequency);

	Font font(L"Consolas");

	bool loop = true;
	unsigned frame = 0;

	QueryPerformanceCounter(&start);
	float prevTime = .0f;
	while (loop) {

		MSG msg;

		hitptr = 0;

		while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
			switch (msg.message) {
			case WM_QUIT:
				loop = false;
				break;
			}
		}

		QueryPerformanceCounter(&current);
		float t = float(double(current.QuadPart - start.QuadPart) / double(frequency.QuadPart));

		float off = 5.f;

		swapBuffers();
	}
	return 0;
}
