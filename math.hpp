
// a dense matrix math implementation heavily (ab)using the curiously recurring template pattern
// frans fredrik frestadius 2016-2018, under the beerware license (rev 42)

// written inspired by http://www.thebestpageintheuniverse.net/c.cgi?u=math

#pragma once
//#pragma warning( once : 4814 ) // don't spam about constexpr not implying const -- the language changed, it's not a mistake to adapt

#include <iostream>
#include <array>

template<typename T> constexpr T constMin(const T a, const T b) { return a <= b ? a : b; }
template<typename T> constexpr T constMax(const T a, const T b) { return a >= b ? a : b; }
template<typename T> constexpr T constAbs(const T a) { return a < T(0) ? -a : a; }
template<typename T> constexpr T constClamp(const T x, const T low, const T high) { return constMin(constMax(x, low), high); }
#define clamp(x,low,high) min(max(x,low),high)

// the type of our reals
typedef float real;
typedef unsigned uint;

// always necessary elemental functions

// natural constants
constexpr real pi = real(3.14159265358979323846264338327950288);

// all of the specific types
template<typename F, int rows, int cols> struct Matrix; // actual data matrix
template<typename F, int rows, int cols, typename I> struct Transpose; // a bunch of different views
template<typename F, int rows, int cols, typename I> struct ConstTranspose;
template<typename F, int rows, int cols, typename I> struct Column;
template<typename F, int rows, int cols, typename I> struct ConstColumn;
template<typename F, int rows, int cols, typename I> struct Row;
template<typename F, int rows, int cols, typename I> struct ConstRow;
template<typename F, int rows, int cols, typename I> struct Diagonal;
template<typename F, int rows, int cols, typename I> struct ConstDiagonal;

// curious recurrence: like virtual functions, but compile time
// F: Field, I: Implementation
template <typename F, int rows, int cols, typename I>
struct AbstractMatrix {

	static_assert(rows > 0 && cols > 0, "no zero dimensions allowed");
	// generic constructor
	inline AbstractMatrix<F, rows, cols, I>() {}

	// a 1-by-1 matrix can be cast to an element
	inline operator F() const { static_assert(rows == 1 && cols == 1, "scalar cast only valid for 1-by-1 matrix"); return (*this)(0, 0); }

	inline bool operator!=(const AbstractMatrix<F, rows, cols, I> & other) { for (auto i = 0; i < cols; ++i) for (auto j = 0; j < rows; ++j) if (static_cast<I&>(*this)(j, i) != other(j, i)) return true; return false; }
	inline bool operator==(const AbstractMatrix<F, rows, cols, I> & other) { for (auto i = 0; i < cols; ++i) for (auto j = 0; j < rows; ++j) if (static_cast<I&>(*this)(j, i) != other(j, i)) return false; return true; }

	// careful: here "other" might be the matrix itself
	template<typename S, typename I2>
	inline AbstractMatrix<F, rows, cols, I>& operator=(const AbstractMatrix<S, rows, cols, I2> & other) {
		auto tmp = Matrix <F, rows, cols>();
		for (auto i = 0; i < cols; ++i) for (auto j = 0; j < rows; ++j) tmp(j, i) = static_cast<const I2&>(other)(j, i);
		for (auto i = 0; i < cols; ++i) for (auto j = 0; j < rows; ++j) static_cast<I&>(*this)(j, i) = tmp(j, i);
		return *this;
	}
	// here it will always be a separate entity in memory
	template<typename S>
	inline AbstractMatrix<F, rows, cols, I>& operator=(const Matrix<S, rows, cols> & other) {
		for (auto i = 0; i < cols; ++i) for (auto j = 0; j < rows; ++j) static_cast<I&>(*this)(j, i) = other(j, i);
		return *this;
	}
	// generic operator()s for indexing
	inline F& operator()(int j, int i) { return static_cast<I&>(*this)(j, i); }
	inline const F& operator()(int j, int i) const { return static_cast<const I&>(*this)(j, i); }
	inline F& operator()(int i) { static_assert(rows == 1 || cols == 1, "single index valid only for vectors"); return static_cast<I&>(*this)(i*(rows > 1), i*(cols > 1)); }
	inline const F& operator()(int i) const { static_assert(rows == 1 || cols == 1, "single index valid only for vectors"); return static_cast<const I&>(*this)(i*(rows > 1), i*(cols > 1)); }
	inline F& operator()() { static_assert(rows == 1 && cols == 1, "no index valid only for 1-by-1"); return static_cast<I&>(*this)(0, 0); }
	inline const F& operator()() const { static_assert(rows == 1 && cols == 1, "no index valid only for 1-by-1"); return static_cast<const I&>(*this)(0, 0); }

	Matrix<F, 1, 1> s(const unsigned& a) { Matrix<F, 1, 1> result;	result(0) = (*this)(a);	return result; }
	Matrix<F, 2, 1> s(const unsigned& a, const unsigned& b) { Matrix<F, 2, 1> result; result(0) = (*this)(a); result(1) = (*this)(b);	return result; }
	Matrix<F, 3, 1> s(const unsigned& a, const unsigned& b, const unsigned& c) { Matrix<F, 3, 1> result; result(0) = (*this)(a); result(1) = (*this)(b); result(2) = (*this)(c); return result; }
	Matrix<F, 4, 1> s(const unsigned& a, const unsigned& b, const unsigned& c, const unsigned& d) { Matrix<F, 4, 1> result; result(0) = (*this)(a); result(1) = (*this)(b); result(2) = (*this)(c); result(3) = (*this)(d); return result; }

	// mult and div by scalar
	inline Matrix<F, rows, cols> operator*(const F & other) const {
		auto result = Matrix<F, rows, cols>();
		for (auto i = 0; i < cols; ++i) for (auto j = 0; j < rows; ++j) result(j, i) = static_cast<const I&>(*this)(j, i) * other;
		return result;
	}
	inline Matrix<F, rows, cols> operator/(const F & other) const {
		auto result = Matrix<F, rows, cols>();
		for (auto i = 0; i < cols; ++i) for (auto j = 0; j < rows; ++j) result(j, i) = static_cast<const I&>(*this)(j, i) / other;
		return result;
	}
	// matrix multiplication
	template<int K, typename I2, typename S>
	inline Matrix<F, rows, K> operator*(const AbstractMatrix<S, cols, K, I2> & other) const {
		auto result = Matrix<F, rows, K>();
		for (auto i = 0; i < rows; ++i)
			for (auto j = 0; j < K; ++j) {
				result(i, j) = F(0);
				for (auto k = 0; k < cols; ++k) result(i, j) += static_cast<const I&>(*this)(i, k)*static_cast<const I2&>(other)(k, j);
			}
		return result;
	}
	// elementwise operations
	template<typename I2>
	inline Matrix<F, rows, cols> operator*(const AbstractMatrix<F, rows, cols, I2> & other) const {
		auto result = Matrix<F, rows, cols>();
		if (rows == cols && rows > 1) // for square, do matrix mult instead (potentially confusing as heck)
			for (auto i = 0; i < rows; ++i) for (auto j = 0; j < cols; ++j) {
				result(i, j) = F(0);
				for (auto k = 0; k < cols; ++k) result(i, j) += static_cast<const I&>(*this)(i, k)*static_cast<const I2&>(other)(k, j);
			}
		else
			for (auto i = 0; i < cols; ++i) for (auto j = 0; j < rows; ++j) result(j, i) = static_cast<const I&>(*this)(j, i) * static_cast<const I2&>(other)(j, i);
		return result;
	}
	template<typename I2>
	inline Matrix<F, rows, cols> operator/(const AbstractMatrix<F, rows, cols, I2> & other) const {
		auto result = Matrix<F, rows, cols>();
		for (auto i = 0; i < cols; ++i) for (auto j = 0; j < rows; ++j) result(j, i) = static_cast<const I&>(*this)(j, i) / static_cast<const I2&>(other)(j, i);
		return result;
	}
	inline Matrix<F, rows, cols> operator-() const {
		auto result = Matrix<F, rows, cols>();
		for (auto i = 0; i < cols; ++i) for (auto j = 0; j < rows; ++j) result(j, i) = -static_cast<const I&>(*this)(j, i);
		return result;
	}
	template<typename I2>
	inline Matrix<F, rows, cols> operator+(const AbstractMatrix<F, rows, cols, I2> & other) const {
		auto result = Matrix<F, rows, cols>();
		for (auto i = 0; i < cols; ++i) for (auto j = 0; j < rows; ++j) result(j, i) = static_cast<const I&>(*this)(j, i) + static_cast<const I2&>(other)(j, i);
		return result;
	}
	template<typename I2>
	inline Matrix<F, rows, cols> operator-(const AbstractMatrix<F, rows, cols, I2> & other) const {
		auto result = Matrix<F, rows, cols>();
		for (auto i = 0; i < cols; ++i) for (auto j = 0; j < rows; ++j) result(j, i) = static_cast<const I&>(*this)(j, i) - static_cast<const I2&>(other)(j, i);
		return result;
	};
	// operator oper= for all of the above
	template<typename I2> inline Matrix<F, rows, cols>& operator+=(const AbstractMatrix<F, rows, cols, I2>& other) { return static_cast<I&>(*this) = static_cast<const I&>(*this) + static_cast<const I2&>(other); }
	template<typename I2> inline Matrix<F, rows, cols>& operator-=(const AbstractMatrix<F, rows, cols, I2>& other) { return static_cast<I&>(*this) = static_cast<const I&>(*this) - static_cast<const I2&>(other); }
	template<typename I2> inline Matrix<F, rows, cols>& operator*=(const AbstractMatrix<F, rows, cols, I2>& other) { return static_cast<I&>(*this) = static_cast<const I&>(*this) * static_cast<const I2&>(other); }
	template<typename I2> inline Matrix<F, rows, cols>& operator/=(const AbstractMatrix<F, rows, cols, I2>& other) { return static_cast<I&>(*this) = static_cast<const I&>(*this) / static_cast<const I2&>(other); }
	inline Matrix<F, rows, cols>& operator*=(const F& other) { return static_cast<I&>(*this) = static_cast<const I&>(*this) * other; }
	inline Matrix<F, rows, cols>& operator/=(const F& other) { return static_cast<I&>(*this) = static_cast<const I&>(*this) / other; }
	template<int K, typename I2> inline Matrix<F, rows, K> operator*=(const AbstractMatrix<F, cols, K, I2> & other) const { return static_cast<I&>(*this) = static_cast<const I&>(*this) * static_cast<const I2&>(other); }
	// generating views: the real benefit (?) of the CRTP
	inline Transpose<F, rows, cols, I> T() { return Transpose<F, rows, cols, I>(*this); }
	inline ConstTranspose<F, rows, cols, I> T() const { return ConstTranspose<F, rows, cols, I>(*this); }
	inline Diagonal<F, rows, cols, I> diag() { return Diagonal<F, rows, cols, I>(*this); }
	inline ConstDiagonal<F, rows, cols, I> diag() const { return ConstDiagonal<F, rows, cols, I>(*this); }
	inline Column<F, rows, cols, I> col(int i) { return Column<F, rows, cols, I>(*this, i); }
	inline ConstColumn<F, rows, cols, I> col(int i) const { return ConstColumn<F, rows, cols, I>(*this, i); }
	inline Row<F, rows, cols, I> row(int i) { return Row<F, rows, cols, I>(*this, i); }
	inline ConstRow<F, rows, cols, I> row(int i) const { return ConstRow<F, rows, cols, I>(*this, i); }
};

template<typename F, int rows, int cols, typename I> inline Matrix<F, rows, cols> operator*(const F & other, const AbstractMatrix<F, rows, cols, I>& matrix) { return matrix * other; }

// define a very arbitrary ordering (necessary for some data structures)
template<typename F, int rows, int cols>
inline bool operator<(const Matrix<F, rows, cols>& a, const Matrix<F, rows, cols>&b) {
	for (auto i = 0; i < cols; ++i)
		for (auto j = 0; j < rows; ++j)
			if (a(j, i) != b(j, i))
				return a(j, i) < b(j, i);
	return a(rows - 1, cols - 1) < b(rows - 1, cols - 1);
}

// union types to allow indexing small vectors neatly with xyzw
template<typename F, int rows, int cols> struct matrix_data { F data[rows*cols]; };
template<typename F> struct matrix_data<F, 1, 1> { matrix_data<F, 1, 1>() {} union { F data[1]; struct { F x; }; }; };
template<typename F> struct matrix_data<F, 1, 2> { matrix_data<F, 1, 2>() {} union { F data[2]; struct { F x, y; }; }; };
template<typename F> struct matrix_data<F, 1, 3> { matrix_data<F, 1, 3>() {} union { F data[3]; struct { F x, y, z; }; }; };
template<typename F> struct matrix_data<F, 1, 4> { matrix_data<F, 1, 4>() {} union { F data[4]; struct { F x, y, z, w; }; }; };
template<typename F> struct matrix_data<F, 2, 1> { matrix_data<F, 2, 1>() {} union { F data[2]; struct { F x, y; }; }; };
template<typename F> struct matrix_data<F, 3, 1> { matrix_data<F, 3, 1>() {} union { F data[3]; struct { F x, y, z; }; }; };
template<typename F> struct matrix_data<F, 4, 1> { matrix_data<F, 4, 1>() {} union { F data[4]; struct { F x, y, z, w; }; }; };

// the data matrix
template<typename F, int rows, int cols>
struct Matrix : public AbstractMatrix < F, rows, cols, Matrix<F, rows, cols>>, public matrix_data <F, rows, cols> {
	using AbstractMatrix<F,rows,cols,Matrix<F,rows,cols>>::operator=; using AbstractMatrix< F, rows, cols, Matrix<F, rows, cols>>::operator();
	inline Matrix<F, rows, cols>() {}
	inline Matrix<F, rows, cols>(const F& a) { for (auto i = 0; i < cols; ++i)for (auto j = 0; j < rows; ++j) this->data[j + i * rows] = a; }

	inline static Matrix<F, rows, cols> identity() { Matrix<F, rows, cols> tmp; for (auto i = 0; i < cols; ++i) for (auto j = 0; j < rows; ++j) if (i == j) tmp.data[j + i * rows] = F(1); else tmp.data[j + i * rows] = F(0); return tmp; }

	template<typename S> inline Matrix<F, rows, cols>(const Matrix<S, rows, cols>& other) { for (auto i = 0; i < cols; ++i) for (auto j = 0; j < rows; ++j) this->data[j + i * rows] = other(j, i); }
	template<typename S, typename I2> inline Matrix<F, rows, cols>(const AbstractMatrix<S, rows, cols, I2>& other) { for (auto i = 0; i < cols; ++i) for (auto j = 0; j < rows; ++j) this->data[j + i * rows] = other(j, i); }

	template<typename S, int rows2, typename I, typename... Var> inline Matrix<F, rows, cols>(const AbstractMatrix<S, rows2, cols, I>& a, Var... args) {
		static_assert(rows2 < rows, "not enough arguments in vector initialization.");
		Matrix<F, rows - rows2, cols> next(args...);
		for (auto i = 0; i < cols; ++i)for (auto j = 0; j < rows; ++j)	this->data[j + i * rows] = (j < rows2) ? a(j, i) : next(j - rows2, i);
	}
	template<typename S, int rows2, typename... Var> inline Matrix<F, rows, cols>(const Matrix<S, rows2, cols>& a, Var... args) {
		static_assert(rows2 < rows, "not enough arguments in vector initialization.");
		Matrix<F, rows - rows2, cols> next(args...);
		for (auto i = 0; i < cols; ++i)for (auto j = 0; j < rows; ++j)	this->data[j + i * rows] = (j < rows2) ? a(j, i) : next(j - rows2, i);
	}

	template<typename S, int cols2, typename I, typename... Var> inline Matrix<F, rows, cols>(const AbstractMatrix<S, rows, cols2, I>& a, Var... args) {
		static_assert(cols2 < cols, "not enough arguments in vector initialization.");
		Matrix<F, rows, cols - cols2> next(args...);
		for (auto i = 0; i < cols; ++i)for (auto j = 0; j < rows; ++j)	this->data[j + i * rows] = (i < cols2) ? a(j, i) : next(j, i - cols2);
	}
	template<typename S, int cols2, typename... Var> inline Matrix<F, rows, cols>(const Matrix<S, rows, cols2>& a, Var... args) {
		static_assert(cols2 < cols, "not enough arguments in vector initialization.");
		Matrix<F, rows, cols - cols2> next(args...);
		for (auto i = 0; i < cols; ++i)for (auto j = 0; j < rows; ++j)	this->data[j + i * rows] = (i < cols2) ? a(j, i) : next(j, i - cols2);
	}

	inline F& operator()(int j, int i) { return this->data[j + i * rows]; }
	inline const F& operator()(int j, int i) const { return this->data[j + i * rows]; }
};

// specialization of rows and columns in order to enable more specific constructors
template<typename F, int rows>
struct Matrix<F, rows, 1> : public AbstractMatrix < F, rows, 1, Matrix<F, rows, 1>>, public matrix_data <F, rows, 1> {
	using AbstractMatrix< F, rows, 1, Matrix<F, rows, 1>>::operator=; using AbstractMatrix< F, rows, 1, Matrix<F, rows, 1>>::operator();
	inline Matrix<F, rows, 1>() {}
	inline Matrix<F, rows, 1>(const F& a) { for (auto j = 0; j < rows; ++j) this->data[j] = a; }

	template<typename S> inline Matrix<F, rows, 1>(const Matrix<S, rows, 1>& other) { for (auto j = 0; j < rows; ++j) this->data[j] = other(j); }
	template<typename S, typename I> inline Matrix<F, rows, 1>(const AbstractMatrix<S, rows, 1, I>& other) { for (auto j = 0; j < rows; ++j) this->data[j] = other(j); }

	template<typename... Var> inline Matrix<F, rows, 1>(const F& a, Var... args) {
		Matrix<F, rows - 1, 1> next(args...);
		for (auto j = 0; j < rows; ++j)	this->data[j] = (j == 0) ? a : next(j - 1);
	}
	template<typename S, int rows2, typename I, typename... Var> inline Matrix<F, rows, 1>(const AbstractMatrix<S, rows2, 1, I>& a, Var... args) {
		static_assert(rows2 < rows, "not enough arguments in vector initialization.");
		Matrix<F, rows - rows2, 1> next(args...);
		for (auto j = 0; j < rows; ++j)	this->data[j] = (j < rows2) ? a(j) : next(j - rows2);
	}
	template<typename S, int rows2, typename... Var> inline Matrix<F, rows, 1>(const Matrix<S, rows2, 1>& a, Var... args) {
		static_assert(rows2 < rows, "not enough arguments in vector initialization.");
		Matrix<F, rows - rows2, 1> next(args...);
		for (auto j = 0; j < rows; ++j)	this->data[j] = (j < rows2) ? a(j) : next(j - rows2);
	}

	inline F& operator()(int j, int i) { return this->data[j]; }
	inline const F& operator()(int j, int i) const { return this->data[j]; }
};

template<typename F, int cols>
struct Matrix<F, 1, cols> : public AbstractMatrix < F, 1, cols, Matrix<F, 1, cols>>, public matrix_data <F, 1, cols> {
	using AbstractMatrix< F, 1, cols, Matrix<F, 1, cols>>::operator=; using AbstractMatrix< F, 1, cols, Matrix<F, 1, cols>>::operator();
	inline Matrix<F, 1, cols>() {}
	inline Matrix<F, 1, cols>(const F& a) { for (auto i = 0; i < cols; ++i) this->data[i] = a; }

	template<typename S> inline Matrix<F, 1, cols>(const Matrix<S, 1, cols>& other) { for (auto j = 0; j < cols; ++j) this->data[j] = other(j); }
	template<typename S, typename I> inline Matrix<F, 1, cols>(const AbstractMatrix<S, 1, cols, I>& other) { for (auto j = 0; j < cols; ++j) this->data[j] = other(j); }

	template<typename... Var> inline Matrix<F, 1, cols>(const F& a, Var... args) {
		Matrix<F, 1, cols - 1> next(args...);
		for (auto j = 0; j < cols; ++j)	this->data[j] = (j == 0) ? a : next(j - 1);
	}
	template<typename S, int cols2, typename I, typename... Var> inline Matrix<F, 1, cols>(const AbstractMatrix<S, 1, cols2, I>& a, Var... args) {
		static_assert(cols2 < cols, "not enough arguments in vector initialization.");
		Matrix<F, 1, cols - cols2> next(args...);
		for (auto j = 0; j < cols; ++j)	this->data[j] = (j < cols2) ? a(j) : next(j - cols2);
	}
	template<typename S, int cols2, typename... Var> inline Matrix<F, 1, cols>(const Matrix<S, 1, cols2>& a, Var... args) {
		static_assert(cols2 < cols, "not enough arguments in vector initialization.");
		Matrix<F, 1, cols - cols2> next(args...);
		for (auto j = 0; j < cols; ++j)	this->data[j] = (j < cols2) ? a(j) : next(j - cols2);
	}

	inline F& operator()(int j, int i) { return this->data[i]; }
	inline const F& operator()(int j, int i) const { return this->data[i]; }
};

// 1-by-1 also needs to be specialized in order for the rows and cols to work
template<typename F>
struct Matrix<F, 1, 1> : public AbstractMatrix < F, 1, 1, Matrix<F, 1, 1>>, public matrix_data <F, 1, 1> {
	using AbstractMatrix< F, 1, 1, Matrix<F, 1, 1>>::operator=; using AbstractMatrix< F, 1, 1, Matrix<F, 1, 1>>::operator();
	inline Matrix<F, 1, 1>() {}
	inline Matrix<F, 1, 1>(const F& a) { this->data[0] = a; }

	template<typename S> inline Matrix<F, 1, 1>(const Matrix<S, 1, 1>& other) { this->data[0] = other(0, 0); }
	template<typename S, typename I2> inline Matrix<F, 1, 1>(const AbstractMatrix<S, 1, 1, I2>& other) { this->data[0] = other(0, 0); }

	template<typename S> operator Matrix<S, 1, 1>() { Matrix<S, 1, 1> result(S(this->data[0])); return result; }
	inline F& operator()(int j, int i) { return this->data[0]; }
	inline const F& operator()(int j, int i) const { return this->data[0]; }
};


// some more sensible names for the usual things

// hlsl equivalents
typedef Matrix<float, 2, 1> float2; typedef Matrix<int, 2, 1> int2; typedef Matrix<unsigned, 2, 1> uint2; typedef Matrix<float, 2, 2> float2x2;
typedef Matrix<float, 3, 1> float3; typedef Matrix<int, 3, 1> int3; typedef Matrix<unsigned, 3, 1> uint3; typedef Matrix<float, 3, 3> float3x3;
typedef Matrix<float, 4, 1> float4; typedef Matrix<int, 4, 1> int4; typedef Matrix<unsigned, 4, 1> uint4; typedef Matrix<float, 4, 4> float4x4;

// for the more glsl-oriented
typedef Matrix<float, 2, 1> vec2; typedef Matrix<int, 2, 1> ivec2; typedef Matrix<unsigned, 2, 1> uvec2; typedef Matrix<float, 2, 2> mat2;
typedef Matrix<float, 3, 1> vec3; typedef Matrix<int, 3, 1> ivec3; typedef Matrix<unsigned, 3, 1> uvec3; typedef Matrix<float, 3, 3> mat3;
typedef Matrix<float, 4, 1> vec4; typedef Matrix<int, 4, 1> ivec4; typedef Matrix<unsigned, 4, 1> uvec4; typedef Matrix<float, 4, 4> mat4;

// semi-neat std::ostream output
template<typename F, int rows, int cols, typename I>
std::ostream& operator<<(std::ostream& out, const AbstractMatrix<F, rows, cols, I>& matrix) {
	out << "\n";
	for (auto i = 0; i < rows; ++i) {
		if (rows > 1 && cols == 1) if (i == 0) out << "{"; else	out << " ";
		for (auto j = 0; j < cols; ++j) {
			if (cols > 1) if (j == 0) out << "{";
			out << F(static_cast<const I&>(matrix)(i, j));
			if (cols > 1) if (j == cols - 1) out << "}"; else out << ", ";
		}
		if (rows > 1 && cols == 1) if (i == rows - 1) out << "}";
		out << " \n";
	}
	return out;
}

// implementations of views: the major difference to the original is always the operator()(int j, int i)
template<typename F, int rows, int cols, typename I>
struct Transpose : public AbstractMatrix < F, cols, rows, Transpose < F, rows, cols, I>> {
	using AbstractMatrix< F, cols, rows, Transpose < F, rows, cols, I>>::operator=; using AbstractMatrix< F, cols, rows, Transpose < F, rows, cols, I>>::operator();
	inline Transpose<F, rows, cols, I>(AbstractMatrix<F, rows, cols, I>& other) : other(other) { }
	inline F& operator()(int j, int i) { return other(i, j); }
	inline const F& operator()(int j, int i) const { return other(i, j); }
private: AbstractMatrix<F, rows, cols, I> & other;
};

// the const versions are pretty much copies
template<typename F, int rows, int cols, typename I>
struct ConstTranspose : public AbstractMatrix < F, cols, rows, ConstTranspose < F, rows, cols, I>> {
	using AbstractMatrix< F, cols, rows, ConstTranspose < F, rows, cols, I>>::operator=; using AbstractMatrix< F, cols, rows, ConstTranspose < F, rows, cols, I>>::operator();
	inline ConstTranspose<F, rows, cols, I>(const AbstractMatrix<F, rows, cols, I> & other) : other(other) { }
	inline const F& operator()(int j, int i) const { return other(i, j); }
private: const AbstractMatrix<F, rows, cols, I> & other;
};

template<typename F, int rows, int cols, typename I>
struct Diagonal : public AbstractMatrix < F, constMin(rows, cols), 1, Diagonal < F, rows, cols, I >> {
	using AbstractMatrix< F, constMin(rows, cols), 1, Diagonal < F, rows, cols, I >>::operator=; using AbstractMatrix< F, constMin(rows, cols), 1, Diagonal < F, rows, cols, I >>::operator();
	inline Diagonal<F, rows, cols, I>(AbstractMatrix<F, rows, cols, I>& other) : other(other) { }
	inline F& operator()(int j, int i) { (void)i; return other(j, j); }
	inline const F& operator()(int j, int i) const { (void)i; return other(j, j); }
private: AbstractMatrix<F, rows, cols, I> & other;
};

template<typename F, int rows, int cols, typename I>
struct ConstDiagonal : public AbstractMatrix < F, constMin(rows, cols), 1, ConstDiagonal < F, rows, cols, I >> {
	using AbstractMatrix< F, constMin(rows, cols), 1, ConstDiagonal < F, rows, cols, I >>::operator=; using AbstractMatrix< F, constMin(rows, cols), 1, ConstDiagonal < F, rows, cols, I >>::operator();
	inline ConstDiagonal<F, rows, cols, I>(const AbstractMatrix<F, rows, cols, I>& other) : other(other) { }
	inline const F& operator()(int j, int i) const { (void)i; return other(j, j); }
private: const AbstractMatrix<F, rows, cols, I> & other;
};

template<typename F, int rows, int cols, typename I>
struct Column : public AbstractMatrix < F, rows, 1, Column < F, rows, cols, I >> {
	using AbstractMatrix< F, rows, 1, Column < F, rows, cols, I >>::operator=; using AbstractMatrix< F, rows, 1, Column < F, rows, cols, I >>::operator();
	inline Column<F, rows, cols, I>(AbstractMatrix<F, rows, cols, I>& other, const int i) : other(other), index(i) { }
	inline F& operator()(int j, int i) { (void)i; return other(j, index); }
	inline const F& operator()(int j, int i) const { (void)i; return other(j, index); }
private: AbstractMatrix<F, rows, cols, I> & other; const int index;
};

template<typename F, int rows, int cols, typename I>
struct ConstColumn : public AbstractMatrix < F, rows, 1, ConstColumn < F, rows, cols, I >> {
	using AbstractMatrix< F, rows, 1, ConstColumn < F, rows, cols, I >>::operator=; using AbstractMatrix< F, rows, 1, ConstColumn < F, rows, cols, I >>::operator();
	inline ConstColumn<F, rows, cols, I>(const AbstractMatrix<F, rows, cols, I>& other, const int i) : other(other), index(i) { }
	inline const F& operator()(int j, int i) const { (void)i; return other(j, index); }
private: const AbstractMatrix<F, rows, cols, I> & other; const int index;
};

template<typename F, int rows, int cols, typename I>
struct Row : public AbstractMatrix < F, 1, cols, Row < F, rows, cols, I>> {
	using AbstractMatrix< F, 1, cols, Row < F, rows, cols, I>>::operator=; using AbstractMatrix< F, 1, cols, Row < F, rows, cols, I>>::operator();
	inline Row < F, rows, cols, I>(AbstractMatrix<F, rows, cols, I>& other, const int i) : other(other), index(i) { }
	inline F& operator()(int j, int i) { (void)j; return other(index, i); }
	inline const F& operator()(int j, int i) const { (void)j; return other(index, i); }
private: AbstractMatrix<F, rows, cols, I> & other; const int index;
};

template<typename F, int rows, int cols, typename I>
struct ConstRow : public AbstractMatrix < F, 1, cols, ConstRow < F, rows, cols, I>> {
	using AbstractMatrix< F, 1, cols, ConstRow < F, rows, cols, I>>::operator=; using AbstractMatrix< F, 1, cols, ConstRow < F, rows, cols, I>>::operator();
	inline ConstRow < F, rows, cols, I>(const AbstractMatrix<F, rows, cols, I>& other, const int i) : other(other), index(i) { }
	inline const F& operator()(int j, int i) const { (void)j; return other(index, i); }
private: const AbstractMatrix<F, rows, cols, I> & other; const int index;
};

//template-recursive determinant
template<typename F, int rows>
struct Determinant {
	inline static F det(const Matrix<F, rows, rows>& A) {
		if (rows == 1) return A(0, 0);
		auto temp = Matrix<F, rows - 1, rows - 1>();
		auto res = F(0);
		for (auto i = 0; i < rows; ++i) {
			auto k = 0;
			for (auto j = 0; j < rows; ++j) {
				if (j == i) continue;
				for (auto l = 0; l < rows - 1; ++l)
					temp(l, k) = A(l + 1, j);
				++k;
			}
			res += A(0, i)*Determinant<F, rows - 1>::det(temp)*(i % 2 ? F(-1) : F(1));
		}
		return res;
	}
};

// no partial specialization of functions in C++; make a struct and call its static method from a global function.
template<typename F> struct Determinant<F, 1> { inline static F det(const Matrix<F, 1, 1>& A) { return F(A); } };
template<typename F, int rows> inline F det(const Matrix<F, rows, rows>& A) { return Determinant<F, rows>::det(A); }

// solve a linear system
template<typename F, int rows, typename I, typename I2>
inline Matrix < F, rows, 1> solve(const AbstractMatrix<F, rows, rows, I>& A, const AbstractMatrix<F, rows, 1, I2>& b) {
	// implicit QR decomposition
	auto R = Matrix<F, rows, rows>(A), G = Matrix<F, rows, rows>(0);
	auto q = Matrix<F, rows, 1>(b);

	G.diag() = Matrix<F, rows, 1>(1.0f);
	for (auto i = 0; i < rows - 1; ++i) {
		for (auto j = i + 1; j < rows; ++j) {
			auto v1 = R(i, i), v2 = R(j, i), l = F(1) / std::sqrt(v1*v1 + v2 * v2); // givens rotation
			auto c = v1 * l, s = -v2 * l;
			G(i, i) = c; G(i, j) = -s;
			G(j, i) = s; G(j, j) = c;
			R = G * R;
			q = G * q;
			G(i, i) = 1.0f; G(i, j) = 0.0f;
			G(j, i) = 0.0f; G(j, j) = 1.0f;
		}
	}

	auto x = Matrix<F, rows, 1>(0);
	for (auto i = rows - 1; i >= 0; --i)
		x(i) = (q(i) - R.row(i)*x) / R(i, i);

	return x;
}

// explicit QR decomposition
template<typename F, int rows, int cols, typename I, typename I2, typename I3>
inline void qr(const AbstractMatrix<F, rows, cols, I>& A, AbstractMatrix<F, rows, rows, I2>& Q, AbstractMatrix<F, rows, cols, I3>& R) {
	auto G = Matrix<F, rows, rows>(0);
	Q = Matrix < F, rows, rows >(0);
	G.diag() = Matrix<F, rows, 1>(1.0f);
	Q.diag() = Matrix<F, rows, 1>(1.0f);
	R = Matrix<F, rows, cols>(A);
	for (auto i = 0; i < cols; ++i) {
		for (auto j = i + 1; j < rows; ++j) {
			auto v1 = R(i, i), v2 = R(j, i), l = F(1) / std::sqrt(v1*v1 + v2 * v2);
			if (v1*v1 + v2 * v2 < 1e-6) continue;
			auto c = v1 * l, s = -v2 * l;
			G(i, i) = c; G(i, j) = -s;
			G(j, i) = s; G(j, j) = c;
			R = G * R;
			Q = Q * G.T();
			G(i, i) = 1.0f; G(i, j) = 0.0f;
			G(j, i) = 0.0f; G(j, j) = 1.0f;
		}
	}
}

// using the QR to solve each column of the identity matrix: produces the inverse
template<typename F, int rows, typename I>
inline Matrix < F, rows, rows> invert(const AbstractMatrix<F, rows, rows, I>& A) {
	// QR decomposition
	auto R = Matrix<F, rows, rows>(), Q = Matrix < F, rows, rows >();
	qr(A, Q, R);

	auto X = Matrix<F, rows, rows>(0);
	for (auto j = 0; j < rows; ++j)
		for (auto i = rows - 1; i >= 0; --i)
			X.col(j)(i) = (Q.row(j)(i) - R.row(i)*X.col(j)) / R(i, i);

	return X;
}

// a weird qr-lp-hack for SVD (slow, only use for small matrices)
template<typename F, int rows, int cols, typename I>
inline void svd(const AbstractMatrix<F, rows, cols, I>& A, Matrix<F, rows, rows>& U, Matrix<F, constMin(rows, cols), 1>& S, Matrix<F, cols, cols>& V) {
	// QR decomposition
	U = Matrix<F, rows, rows>(.0f);
	auto K = Matrix<F, rows, cols>(A);
	V = Matrix < F, cols, cols >(.0f);

	U.diag() = Matrix<F, rows, 1>(1);
	V.diag() = Matrix<F, cols, 1>(1);

	auto R = Matrix<F, rows, cols>();
	auto Q = Matrix < F, rows, rows >();
	auto L = Matrix<F, cols, rows>();
	auto P = Matrix < F, cols, cols >();

	for (auto i = 0; i < rows*cols * 4; ++i) { // TODO: analysis on the amount of necessary iterations
		qr(K, Q, R);
		qr(R.T(), P, L.T().T());
		U = U * Q;
		K = L.T();
		V = P.T() * V;
	}
	S = K.diag();
}

// 3-dimensional cross product
template<typename F>
inline Matrix<F, 3, 1> cross(const Matrix<F, 3, 1>&a, const Matrix<F, 3, 1>& b) {
	return Matrix<F, 3, 1>{a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x};
}

// squared l2 norm
template<typename F, int rows, int cols, typename I>
inline F squaredLength(const AbstractMatrix<F, rows, cols, I>& val) {
	auto sum = F(0);
	for (auto i = 0; i < cols; ++i) for (auto j = 0; j < rows; ++j) sum += val(j, i)*val(j, i);
	return sum;
}

// l2 norm
template<typename F, int rows, int cols, typename I>
inline F length(const AbstractMatrix<F, rows, cols, I>& val) {
	return std::sqrt(squaredLength(val));
}

//normalization
template<typename F, int rows, int cols, typename I>
inline Matrix<F, rows, cols> normalize(const AbstractMatrix<F, rows, cols, I>& val) {
	return val / length(val);
}

// typical projection matrix. divide by w afterwards to get projected coordinates.
template<typename F>
inline Matrix<F, 4, 4> projection(const F& w_over_h, const F& fov, const F& zNear = 1 / F(10), const F& zFar = F(20)) {
	Matrix<F, 4, 4> result(F(0));

	F r = F(1) / std::tan(fov / F(2) * pi / F(180));

	result(0, 0) = r / w_over_h;
	result(1, 1) = r;
	result(2, 2) = (zFar+zNear) / (zNear - zFar);
	result(2, 3) = F(2) * zFar * zNear / (zNear - zFar);
	result(3, 2) = -F(1);

	return result;
}

// ortho projection with a sensible z scale for z-buffering
template<typename F>
inline Matrix<F, 4, 4> ortho(const F& w_over_h, const F& size, const F& zNear = 1 / F(10), const F& zFar = F(20)) {
	Matrix<F, 4, 4> result(F(0));

	result(0, 0) = F(1) / (size*w_over_h);
	result(1, 1) = F(1) /size;
	result(2, 2) = -F(2) / (zFar - zNear);
	result(2, 3) = -(zFar+zNear) / (zFar - zNear);
	result(3, 3) = F(1);

	return result;
}

// construct a 
template<typename F>
inline Matrix<F, 4, 4> cameraToWorld(const Matrix<F, 3, 1>& position, const Matrix<F, 3, 1>& target, const Matrix<F, 3, 1>& up = Matrix<F, 3, 1>(F(0), F(1), F(0))) {
	Matrix<F, 3, 1> direction = normalize(target - position), right = normalize(cross(direction, up)), newUp = cross(right, direction);
	return Matrix<F, 4, 4>(
		Matrix<F, 4, 1>(right, .0f),
		Matrix<F, 4, 1>(newUp, .0f),
		Matrix<F, 4, 1>(-direction, .0f),
		Matrix<F, 4, 1>(position, 1.0f));
}

template<typename F>
inline Matrix<F, 4, 4> worldToCamera(const Matrix<F, 3, 1>& position, const Matrix<F, 3, 1>& target, const Matrix<F, 3, 1>& up = Matrix<F, 3, 1>(F(0), F(1), F(0))) {
	return inverse(cameraToWorld(position, target, up));
}

// rotations around the major axes
template<typename F>
inline Matrix<F, 3, 3> rotation_x(const F& angle) {
	Matrix<F, 3, 3> result(F(0));
	F cosa = cos(angle), sina = sin(angle);
	result(1, 1) = cosa;
	result(1, 2) = -sina;
	result(2, 1) = sina;
	result(2, 2) = cosa;
	result(0, 0) = F(1);
	return result;
}
template<typename F>
inline Matrix<F, 3, 3> rotation_y(const F& angle) {
	Matrix<F, 3, 3> result(F(0));
	F cosa = cos(angle), sina = sin(angle);
	result(0, 0) = cosa;
	result(0, 2) = -sina;
	result(2, 0) = sina;
	result(2, 2) = cosa;
	result(1, 1) = F(1);
	return result;
}
template<typename F>
inline Matrix<F, 3, 3> rotation_z(const F& angle) {
	Matrix<F, 3, 3> result(F(0));
	F cosa = cos(angle), sina = sin(angle);
	result(0, 0) = cosa;
	result(0, 1) = -sina;
	result(1, 0) = sina;
	result(1, 1) = cosa;
	result(2, 2) = F(1);
	return result;
}

template<typename F>
inline Matrix<F, 4, 4> xRotate(const F& angle) {
	Matrix<F, 4, 4> result(F(0));
	F cosa = cos(angle), sina = sin(angle);
	result(1, 1) = cosa;
	result(1, 2) = -sina;
	result(2, 1) = sina;
	result(2, 2) = cosa;
	result(0, 0) = result(3, 3) = F(1);
	return result;
}
template<typename F>
inline Matrix<F, 4, 4> yRotate(const F& angle) {
	Matrix<F, 4, 4> result(F(0));
	F cosa = cos(angle), sina = sin(angle);
	result(0, 0) = cosa;
	result(0, 2) = -sina;
	result(2, 0) = sina;
	result(2, 2) = cosa;
	result(1, 1) = result(3, 3) = F(1);
	return result;
}
template<typename F>
inline Matrix<F, 4, 4> zRotate(const F& angle) {
	Matrix<F, 4, 4> result(F(0));
	F cosa = cos(angle), sina = sin(angle);
	result(0, 0) = cosa;
	result(0, 1) = -sina;
	result(1, 0) = sina;
	result(1, 1) = cosa;
	result(2, 2) = result(3, 3) = F(1);
	return result;
}
template<typename F, int dimension>
inline Matrix<F, dimension + 1, dimension + 1> translate(const Matrix<F, dimension, 1>& offset) {
	Matrix<F, dimension + 1, dimension + 1> result(F(0));
	for (int i = 0; i < dimension + 1; ++i)
		result(i, i) = F(1);
	for (int i = 0; i < dimension; ++i)
		result(i, dimension) = offset(i);
	return result;
}
