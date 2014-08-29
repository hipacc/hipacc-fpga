
/**
 * \brief Filter class.
 * \author Nicolas Apelt
 * \date 2013, 2014
 *
 */
#pragma once

#include <ap_int.h>
#include <hls_stream.h>
#include <assert.h>
#define ASSERTION_CHECK

#define GROUP_DELAY  (KERNEL_SIZE/2)

/**
 * LineBuffer
 */
template<
	int WIDTH,
	int HEIGHT,
	typename T
>
class LineBuffer
{
private:
	T buf[WIDTH][HEIGHT];

public:
	LineBuffer() {
#pragma HLS ARRAY_PARTITION variable=buf complete dim=2
		//fill(0);
	}

	void fill(const T val)
	{
		for (int x = 0; x < WIDTH; ++x)
			for (int y = 0; y < HEIGHT; ++y)
#pragma HLS LOOP_FLATTEN
				buf[x][y] = val;
	}

	/**
	 * Sets value at given coordinates.
	 * @param value New value to be set.
	 * @param x Column
	 * @param y Row
	 */
	void setAt(const T value, const int x, const int y) {
#pragma HLS INLINE
#ifdef ASSERTION_CHECK
		assert(x >= 0); assert(x < WIDTH); assert(y >= 0); assert(y < HEIGHT);
#endif
		buf[x][y] = value;
	}

	/**
	 * Return value at given coordinates.
	 * @param x Row
	 * @param y Column
	 * @return Value at given coordinates.
	 */
	T getAt(const int x, const int y) const {
#pragma HLS INLINE
#ifdef ASSERTION_CHECK
		assert(x >= 0); assert(x < WIDTH); assert(y >= 0); assert(y < HEIGHT);
#endif
		return buf[x][y];
	}

	/**
	 * Return all vectors of the given column as array.
	 * @param x Column
	 */
	const T* getColumn(const int x) const {
#pragma HLS INLINE
#ifdef ASSERTION_CHECK
		assert(x >= 0); assert(x < WIDTH);
#endif
		return buf[x];
	}

	/**
	 * Insert a new value at the bottom of the given column, shifting up all values.
	 * @param x Column
	 * @param new_value The value to be inserted.
	 */
	void insertTop(const int x, const T new_value) {
#ifdef ASSERTION_CHECK
		assert(x >= 0); assert(x < WIDTH);
#endif
		for (int yy = HEIGHT - 1; yy > 0; --yy)
			buf[x][yy] = buf[x][yy-1];

		buf[x][0] = new_value;
	}

	/**
	 * Insert a new value at the top of the given column, shifting down all values.
	 * @param x Column
	 * @param new_value The value to be inserted.
	 */
	void insertBottom(const int x, const T new_value) {
#ifdef ASSERTION_CHECK
		assert(x >= 0); assert(x < WIDTH);
#endif
		for (int yy = 0; yy < HEIGHT - 1; ++yy)
			buf[x][yy] = buf[x][yy+1];

		buf[x][HEIGHT-1] = new_value;
	}

	//----
	/**
	 * FOR DEBUGGING PURPOSE ONLY: Print the current window values onto stdout.
	 */
	/*void print(const int width = WIDTH, const int height = HEIGHT) const {
		for (int y = 0; y < width; ++y) {
			for (int x = 0; x < height; ++x)
				printf(" %8.3f", buf[x][y]);

			printf("\n");
		}
	}*/
};

/**
 * Window
 */
template<
	int WIDTH,
	int HEIGHT,
	typename T
>
class Window
{
private:
	T buf[WIDTH][HEIGHT];

public:
	Window() {
#pragma HLS ARRAY_PARTITION variable=buf complete dim=0
	}

	void setAt(const T value, const int x, const int y) {
#pragma HLS INLINE
#ifdef ASSERTION_CHECK
		assert(x >= 0); assert(x < WIDTH); assert(y >= 0); assert(y < HEIGHT);
#endif
		buf[x][y] = value;
	}

	T getAt(const int x, const int y) const {
#pragma HLS INLINE
#ifdef ASSERTION_CHECK
		assert(x >= 0); assert(x < WIDTH); assert(y >= 0); assert(y < HEIGHT);
#endif
		return buf[x][y];
	}

	void shiftDown() {
		for (int y = HEIGHT - 1; y > 0; --y)
			for (int x = 0; x < WIDTH; ++x)
#pragma HLS LOOP_FLATTEN
				buf[x][y] = buf[x][y-1];
	}

	void shiftUp() {
		for (int y = 0; y < HEIGHT - 1; ++y)
			for (int x = 0; x < WIDTH; ++x)
#pragma HLS LOOP_FLATTEN
				buf[x][y] = buf[x][y+1];
	}

	void shiftLeft() {
		for (int y = 0; y < HEIGHT; ++y)
			for (int x = 0; x < WIDTH - 1; ++x)
#pragma HLS LOOP_FLATTEN
				buf[x][y] = buf[x+1][y];
	}

	void shiftRight() {
		for (int y = 0; y < HEIGHT; ++y)
			for (int x = WIDTH - 1; x > 0; --x)
#pragma HLS LOOP_FLATTEN
				buf[x][y] = buf[x-1][y];
	}

	void insertTop(const T new_values[WIDTH]) {
		shiftDown();

		for (int x = 0; x < WIDTH; ++x)
			buf[x][0] = new_values[x];
	}

	void insertBottom(const T new_values[WIDTH]) {
		shiftUp();

		for (int x = 0; x < WIDTH; ++x)
			buf[x][HEIGHT-1] = new_values[x];
	}

	void insertRight(const T new_values[HEIGHT]) {
		shiftLeft();

		for (int y = 0; y < HEIGHT; ++y)
			buf[WIDTH-1][y] = new_values[y];
	}

	void insertLeft(const T new_values[HEIGHT]) {
		shiftRight();

		for (int y = 0; y < HEIGHT; ++y)
			buf[0][y] = new_values[y];
	}

	void setColumn(const T new_values[HEIGHT], const int x) {
		assert(x >= 0); assert(x < WIDTH);

		for (int y = 0; y < HEIGHT; ++y)
			buf[x][y] = new_values[y];
	}

	/**
	 * Fills the complete window with the given value.
	 * @param val The filling value
	 */
	void fill(const T val) {
		for (int y = 0; y < HEIGHT; ++y) {
			for (int x = 0; x < WIDTH; ++x)
#pragma HLS LOOP_FLATTEN
				buf[x][y] = val;
		}
	}

	/**
	 * Scale (multiply) the whole window with the given factor.
	 * @param fact Scaling factor
	 */
	void scale(const T fact) {
		for (int y = 0; y < HEIGHT; ++y)
			for (int x = 0; x < WIDTH; ++x)
#pragma HLS LOOP_FLATTEN
				buf[x][y] *= fact;
	}

	/**
	 * Add a scalar to the whole window (offsetting).
	 * @param val The offset.
	 */
	void addScalar(const T val) {
		for (int y = 0; y < HEIGHT; ++y)
			for (int x = 0; x < WIDTH; ++x)
#pragma HLS LOOP_FLATTEN
				buf[x][y] += val;
	}

	/**
	 * Calculates the weighted sum of all values in the window.
	 * @param w Sum weights (must be a window of equal size).
	 * @return The weighted sum of the current window values.
	 */
	T weightedSum(const Window<WIDTH, HEIGHT, T> &w) const {
		T sum = (T) 0;

		for (int y = 0; y < HEIGHT; ++y)
			for (int x = 0; x < WIDTH; ++x)
#pragma HLS LOOP_FLATTEN
				sum += buf[x][y] * w.buf[x][y];

		return sum;
	}

	/**
	 * @return The maximum number within the current window.
	 */
	T max() const {
		T v = buf[0][0];

		for (int y = 0; y < HEIGHT; ++y)
			for (int x = 0; x < WIDTH; ++x)
#pragma HLS LOOP_FLATTEN
				if (buf[x][y] > v)
					v = buf[x][y];

		return v;
	}

	/**
	 * @return The minimum number within the current window.
	 */
	T min() const {
		T v = buf[0][0];

		for (int y = 0; y < HEIGHT; ++y)
			for (int x = 0; x < WIDTH; ++x)
#pragma HLS LOOP_FLATTEN
				if (buf[x][y] < v)
					v = buf[x][y];

		return v;
	}

//----
	/**
	 * FOR DEBUGGING PURPOSE ONLY: Print the current window values onto stdout.
	 */
	void print() const {
		/*for (int y = 0; y < HEIGHT; ++y) {
			for (int x = 0; x < WIDTH; ++x)
				printf(" %8.3f", buf[x][y]);

			printf("\n");
		}*/
	}
};

// Border Handling Enums
struct BorderPadding {
public:
    enum values {
    	BORDER_CONST,
    	BORDER_REPEAT,
    	BORDER_REFLECT,
    	BORDER_REFLECT_101
    };

    typedef void isBorderMode;
};

/*
class BORDER_CONST       : public BorderPadding { public: static const values value = BorderPadding::BORDER_CONST; };
class BORDER_REPEAT      : public BorderPadding { public: static const values value = BorderPadding::BORDER_REPEAT; };
class BORDER_REFLECT     : public BorderPadding { public: static const values value = BorderPadding::BORDER_REFLECT; };
class BORDER_REFLECT_101 : public BorderPadding { public: static const values value = BorderPadding::BORDER_REFLECT_101; };
class BORDER_DEFAULT     : public BorderPadding { public: static const values value = BorderPadding::BORDER_REFLECT_101; };
*/

/**
 * Applies the given filter to the input stream and writes the result
 * to the given output stream.
 * @param in_s The input stream.
 * @param out_s The output stream.
 * @param filter An arbitrary class implementing the operator()(const Window<KERNEL_SIZE, KERNEL_SIZE, T> &wnd) which is called for filtering.
 */
static int getExtrapolationCoord(const int offset, const int length, const enum BorderPadding::values borderPadding)
{
	// Instantiation w.r.t. borderPadding unnecessary b/c borderPadding is const in whole program
#pragma HLS FUNCTION_INSTANTIATE variable=offset

#ifdef ASSERTION_CHECK
	assert(length > 0);
	assert( (borderPadding == BorderPadding::BORDER_REFLECT) ||
			(borderPadding == BorderPadding::BORDER_REFLECT_101) ||
			(borderPadding == BorderPadding::BORDER_REPEAT) ||
			(borderPadding == BorderPadding::BORDER_CONST) );
#endif

	if (offset < 0) {
		switch (borderPadding) {
		case BorderPadding::BORDER_REPEAT:
			return 0;
		case BorderPadding::BORDER_REFLECT:
			// TODO check
			return -offset - 1;
		case BorderPadding::BORDER_REFLECT_101:
			return -offset;
		case BorderPadding::BORDER_CONST:
		default:	// just for compiler convenience
			// TODO
			return -1;
		}
	} else {
		if (offset < length) {
			return offset;
		} else { // if (offset >= length)
			switch (borderPadding) {
			case BorderPadding::BORDER_REPEAT:
				// TODO check
				return length - 1;
			case BorderPadding::BORDER_REFLECT:
				// TODO check
				return (length - 1) - (offset - length);
			case BorderPadding::BORDER_REFLECT_101:
				// TODO check
				return (length - 1) - (offset - length) - 1;
			case BorderPadding::BORDER_CONST:
			default:	// just for compiler convenience
				// TODO
				return -1;
			}
		}
	}
}

template<int KERNEL_SIZE, typename T>
void handleBorders(
		Window<KERNEL_SIZE, KERNEL_SIZE, T> &wnd,
		const int &x,
		const int &y,
		const int &width,
		const int &height,
		const enum BorderPadding::values borderPadding)
{
#pragma HLS FUNCTION_INSTANTIATE variable=borderPadding

	/*
	 * Border Handling: x-direction
	 */
	if (x == GROUP_DELAY) {
		for (int xx = -GROUP_DELAY; xx < 0; ++xx) {
			int ix;

			if (borderPadding != BorderPadding::BORDER_CONST)
				ix = getExtrapolationCoord(xx, KERNEL_SIZE, borderPadding);

			for (int yy = 0; yy < KERNEL_SIZE; ++yy) {
				if (borderPadding == BorderPadding::BORDER_CONST)
					wnd.setAt(BORDER_FILL_VALUE, xx+GROUP_DELAY, yy);
				else
					wnd.setAt(wnd.getAt(ix+GROUP_DELAY, yy), xx+GROUP_DELAY, yy);
			}
		}
	} else if (x >= width) {
		for (int xx = 0; xx < KERNEL_SIZE; ++xx) {
			int ix;

			if (borderPadding != BorderPadding::BORDER_CONST)
				ix = getExtrapolationCoord(xx, KERNEL_SIZE - 1 - (x - width), borderPadding);

			for (int yy = 0; yy < KERNEL_SIZE; ++yy)
				if (borderPadding == BorderPadding::BORDER_CONST)
					wnd.setAt(BORDER_FILL_VALUE, xx, yy);
				else
					wnd.setAt(wnd.getAt(ix, yy), xx, yy);
		}
	}

	/*
	 * Border Handling: y-direction
	 */
	if ((y >= GROUP_DELAY) && (y < KERNEL_SIZE-1)) {
		for (int yy = -GROUP_DELAY; yy < GROUP_DELAY; ++yy) {
			int iy;

			if (borderPadding != BorderPadding::BORDER_CONST)
				iy = getExtrapolationCoord(yy, KERNEL_SIZE, borderPadding);

			for (int xx = 0; xx < KERNEL_SIZE; ++xx) {
				if (borderPadding == BorderPadding::BORDER_CONST)
					wnd.setAt(BORDER_FILL_VALUE, xx, yy + GROUP_DELAY);
				else
					wnd.setAt(wnd.getAt(xx, iy + GROUP_DELAY), xx, yy + GROUP_DELAY);
			}
		}
	} else if (y >= height) {
		for (int yy = GROUP_DELAY+1; yy < KERNEL_SIZE; ++yy) {
			int iy;

			if (borderPadding != BorderPadding::BORDER_CONST)
				iy = getExtrapolationCoord(yy, KERNEL_SIZE - 1 - (y - height), borderPadding);

			for (int xx = 0; xx < KERNEL_SIZE; ++xx) {
				if (borderPadding == BorderPadding::BORDER_CONST)
					wnd.setAt(BORDER_FILL_VALUE, xx, yy);
				else
					wnd.setAt(wnd.getAt(xx, iy), xx, yy);
			}
		}
	}
}

template<int KERNEL_SIZE, typename T>
void handleBorders2(
		Window<KERNEL_SIZE, KERNEL_SIZE, T> &wnd,
		Window<KERNEL_SIZE, KERNEL_SIZE, T> &wnd2,
		const int &x,
		const int &y,
		const int &width,
		const int &height,
		const enum BorderPadding::values borderPadding)
{
#pragma HLS FUNCTION_INSTANTIATE variable=borderPadding

	/*
	 * Border Handling: x-direction
	 */
	if (x == GROUP_DELAY) {
		for (int xx = -GROUP_DELAY; xx < 0; ++xx) {
			int ix;

			if (borderPadding != BorderPadding::BORDER_CONST)
				ix = getExtrapolationCoord(xx, KERNEL_SIZE, borderPadding);

			for (int yy = 0; yy < KERNEL_SIZE; ++yy) {
				if (borderPadding == BorderPadding::BORDER_CONST) {
					wnd.setAt(BORDER_FILL_VALUE, xx+GROUP_DELAY, yy);
					wnd2.setAt(BORDER_FILL_VALUE, xx+GROUP_DELAY, yy);
				} else {
					wnd.setAt(wnd.getAt(ix+GROUP_DELAY, yy), xx+GROUP_DELAY, yy);
					wnd2.setAt(wnd2.getAt(ix+GROUP_DELAY, yy), xx+GROUP_DELAY, yy);
				}
			}
		}
	} else if (x >= width) {
		for (int xx = 0; xx < KERNEL_SIZE; ++xx) {
			int ix;

			if (borderPadding != BorderPadding::BORDER_CONST)
				ix = getExtrapolationCoord(xx, KERNEL_SIZE - 1 - (x - width), borderPadding);

			for (int yy = 0; yy < KERNEL_SIZE; ++yy)
				if (borderPadding == BorderPadding::BORDER_CONST) {
					wnd.setAt(BORDER_FILL_VALUE, xx, yy);
					wnd2.setAt(BORDER_FILL_VALUE, xx, yy);
				} else {
					wnd.setAt(wnd.getAt(ix, yy), xx, yy);
					wnd2.setAt(wnd2.getAt(ix, yy), xx, yy);
				}
		}
	}

	/*
	 * Border Handling: y-direction
	 */
	if ((y >= GROUP_DELAY) && (y < KERNEL_SIZE-1)) {
		for (int yy = -GROUP_DELAY; yy < GROUP_DELAY; ++yy) {
			int iy;

			if (borderPadding != BorderPadding::BORDER_CONST)
				iy = getExtrapolationCoord(yy, KERNEL_SIZE, borderPadding);

			for (int xx = 0; xx < KERNEL_SIZE; ++xx) {
				if (borderPadding == BorderPadding::BORDER_CONST) {
					wnd.setAt(BORDER_FILL_VALUE, xx, yy + GROUP_DELAY);
					wnd2.setAt(BORDER_FILL_VALUE, xx, yy + GROUP_DELAY);
				} else {
					wnd.setAt(wnd.getAt(xx, iy + GROUP_DELAY), xx, yy + GROUP_DELAY);
					wnd2.setAt(wnd2.getAt(xx, iy + GROUP_DELAY), xx, yy + GROUP_DELAY);
				}
			}
		}
	} else if (y >= height) {
		for (int yy = GROUP_DELAY+1; yy < KERNEL_SIZE; ++yy) {
			int iy;

			if (borderPadding != BorderPadding::BORDER_CONST)
				iy = getExtrapolationCoord(yy, KERNEL_SIZE - 1 - (y - height), borderPadding);

			for (int xx = 0; xx < KERNEL_SIZE; ++xx) {
				if (borderPadding == BorderPadding::BORDER_CONST) {
					wnd.setAt(BORDER_FILL_VALUE, xx, yy);
					wnd2.setAt(BORDER_FILL_VALUE, xx, yy);
				} else {
					wnd.setAt(wnd.getAt(xx, iy), xx, yy);
					wnd2.setAt(wnd2.getAt(xx, iy), xx, yy);
				}
			}
		}
	}
}

/*
 * Process (SISO)
 */
template<int KERNEL_SIZE, typename IN, typename OUT, class Filter>
void process(
		hls::stream<IN> &in_s,
		hls::stream<OUT> &out_s,
		const int &width,
		const int &height,
		Filter &filter,
		const enum BorderPadding::values borderPadding)
{
	// TODO fix this
#ifdef ASSERTION_CHECK
	assert( width <= MAX_WIDTH ); assert( height <= MAX_HEIGHT );
	assert( (KERNEL_SIZE % 2) == 1 );

/*
	// TODO strangest bug ever!!
	assert( (borderPadding == BorderPadding::BORDER_REFLECT) ||
			(borderPadding == BorderPadding::BORDER_REFLECT_101) ||
			(borderPadding == BorderPadding::BORDER_REPEAT) ||
			(borderPadding == BorderPadding::BORDER_CONST) );
*/
#endif

	LineBuffer<MAX_WIDTH, KERNEL_SIZE - 1, IN> lb;
	Window<KERNEL_SIZE, KERNEL_SIZE, IN> wnd;

process_main_loop:
	for (int y = 0; y < height + GROUP_DELAY; ++y) {
		for (int x = 0; x < MAX_WIDTH + GROUP_DELAY; ++x) {
PRAGMA_HLS(HLS pipeline ii=II_TARGET)
#pragma HLS INLINE region

			if (x >= width + GROUP_DELAY ||
			    y >= height + GROUP_DELAY)
				continue;

			wnd.shiftLeft();

			if (x < width) {
				IN new_val;

				// The topmost value is not shifted, so just copy it separately
				wnd.setAt(lb.getAt(x, 0), KERNEL_SIZE-1, 0);

				// Shift current column in LineBuffer one row up and copy it to the Window simultaneously
				for (int yy = 0; yy < KERNEL_SIZE - 2; ++yy) {
					const IN tmp = lb.getAt(x, yy+1);
					wnd.setAt(tmp, KERNEL_SIZE-1, yy+1);
					lb.setAt(tmp, x, yy);
				}

				if (y < height)
					new_val = in_s.read();
				else
					new_val = 0;

				// Insert new value at the bottom of both buffers
				lb.setAt(new_val, x, KERNEL_SIZE-2);
				wnd.setAt(new_val, KERNEL_SIZE-1, KERNEL_SIZE-1);
			}

			handleBorders(wnd, x, y, width, height, borderPadding);
/*
			if ((x >= GROUP_DELAY) && (y >= GROUP_DELAY)) {
				printf("x=%d, y=%d:\n", x, y);
				wnd.print();
				printf("\n");
			}
*/
			// Do the filtering
			if ((x >= GROUP_DELAY && x < width+GROUP_DELAY) && (y >= GROUP_DELAY && height+GROUP_DELAY))
				out_s << filter(wnd);
		}
	}
}

/*
 * Process (MISO)
 */
template<int KERNEL_SIZE, typename IN, typename OUT, class Filter>
void process2(
		hls::stream<IN> &in_s,
		hls::stream<IN> &in_s2,
		hls::stream<OUT> &out_s,
		const int &width,
		const int &height,
		Filter &filter,
		const enum BorderPadding::values borderPadding)
{
	// TODO fix this
#ifdef ASSERTION_CHECK
	assert( width <= MAX_WIDTH ); assert( height <= MAX_HEIGHT );
	assert( (KERNEL_SIZE % 2) == 1 );

/*
	// TODO strangest bug ever!!
	assert( (borderPadding == BorderPadding::BORDER_REFLECT) ||
			(borderPadding == BorderPadding::BORDER_REFLECT_101) ||
			(borderPadding == BorderPadding::BORDER_REPEAT) ||
			(borderPadding == BorderPadding::BORDER_CONST) );
*/
#endif

	LineBuffer<MAX_WIDTH, KERNEL_SIZE - 1, IN> lb;
	Window<KERNEL_SIZE, KERNEL_SIZE, IN> wnd;
	LineBuffer<MAX_WIDTH, KERNEL_SIZE - 1, IN> lb2;
	Window<KERNEL_SIZE, KERNEL_SIZE, IN> wnd2;

process_main_loop:
	for (int y = 0; y < height + GROUP_DELAY; ++y) {
		for (int x = 0; x < MAX_WIDTH + GROUP_DELAY; ++x) {
PRAGMA_HLS(HLS pipeline ii=II_TARGET)
#pragma HLS INLINE region

			if (x >= width + GROUP_DELAY ||
				y >= height + GROUP_DELAY)
				continue;

			wnd.shiftLeft();
			wnd2.shiftLeft();

			if (x < width) {
				IN new_val;
				IN new_val2;

				// The topmost value is not shifted, so just copy it separately
				wnd.setAt(lb.getAt(x, 0), KERNEL_SIZE-1, 0);
				wnd2.setAt(lb2.getAt(x, 0), KERNEL_SIZE-1, 0);

				// Shift current column in LineBuffer one row up and copy it to the Window simultaneously
				for (int yy = 0; yy < KERNEL_SIZE - 2; ++yy) {
					const IN tmp = lb.getAt(x, yy+1);
					const IN tmp2 = lb2.getAt(x, yy+1);
					wnd.setAt(tmp, KERNEL_SIZE-1, yy+1);
					lb.setAt(tmp, x, yy);
					wnd2.setAt(tmp2, KERNEL_SIZE-1, yy+1);
					lb2.setAt(tmp2, x, yy);
				}

				if (y < height) {
					new_val = in_s.read();
					new_val2 = in_s2.read();
				} else {
					new_val = 0;
					new_val2 = 0;
				}

				// Insert new value at the bottom of both buffers
				lb.setAt(new_val, x, KERNEL_SIZE-2);
				wnd.setAt(new_val, KERNEL_SIZE-1, KERNEL_SIZE-1);
				lb2.setAt(new_val2, x, KERNEL_SIZE-2);
				wnd2.setAt(new_val2, KERNEL_SIZE-1, KERNEL_SIZE-1);
			}

			handleBorders2(wnd, wnd2, x, y, width, height, borderPadding);
/*
			if ((x >= GROUP_DELAY) && (y >= GROUP_DELAY)) {
				printf("x=%d, y=%d:\n", x, y);
				wnd.print();
				printf("\n");
			}
*/
			// Do the filtering
			if ((x >= GROUP_DELAY && x < width+GROUP_DELAY) && (y >= GROUP_DELAY && height+GROUP_DELAY))
				out_s << filter(wnd, wnd2);
		}
	}
}

/*
 * Process (SIMO (Out: 2x))
 *
 * Filter:
 *   void operator() (Window<KERNEL_SIZE, KERNEL_SIZE, T> &wnd, T &out1, T &out2);
 */
template<int KERNEL_SIZE, typename IN, typename OUT, class Filter>
void processSIMO(
		hls::stream<IN> &in_s,
		hls::stream<OUT> &out1_s,
		hls::stream<OUT> &out2_s,
		const int &width,
		const int &height,
		Filter &filter,
		const enum BorderPadding::values borderPadding)
{
#ifdef ASSERTION_CHECK
	assert( width <= MAX_WIDTH ); assert(height <= MAX_HEIGHT);
	assert( (KERNEL_SIZE % 2) == 1 );
#endif

	LineBuffer<MAX_WIDTH, KERNEL_SIZE - 1, IN> lb;
	Window<KERNEL_SIZE, KERNEL_SIZE, IN> wnd;

	for (int y = 0; y < height + GROUP_DELAY; ++y) {
		for (int x = 0; x < MAX_WIDTH + GROUP_DELAY; ++x) {
PRAGMA_HLS(HLS pipeline ii=II_TARGET)
#pragma HLS INLINE region

			if (x >= width + GROUP_DELAY)
				continue;

			wnd.shiftLeft();

			if (x < width) {
				IN new_val;

				// The topmost value is not shifted, so just copy it separately
				wnd.setAt(lb.getAt(x, 0), KERNEL_SIZE-1, 0);

				// Shift current column in LineBuffer one row up and copy it to the Window simultaneously
				for (int yy = 0; yy < KERNEL_SIZE - 2; ++yy) {
					const IN tmp = lb.getAt(x, yy+1);
					wnd.setAt(tmp, KERNEL_SIZE-1, yy+1);
					lb.setAt(tmp, x, yy);
				}

				if (y < height)
					new_val = in_s.read();
				else
					new_val = 0;

				// Insert new value at the bottom of both buffers
				lb.setAt(new_val, x, KERNEL_SIZE-2);
				wnd.setAt(new_val, KERNEL_SIZE-1, KERNEL_SIZE-1);
			}

			handleBorders(wnd, x, y, width, height, borderPadding);

			// Do the filtering
			if ((x >= GROUP_DELAY && x < width+GROUP_DELAY) && (y >= GROUP_DELAY && height+GROUP_DELAY)) {
				OUT out1, out2;

				filter(wnd, out1, out2);

				out1_s << out1;
				out2_s << out2;
			}
		}
	}
}

//---
template<int KERNEL_SIZE, typename IN, typename OUT, class Filter>
void processPixels(
		hls::stream<IN> &in_s,
		hls::stream<OUT> &out_s,
		const int &width,
		const int &height,
		Filter &filter)
{
	assert(width <= MAX_WIDTH); assert(height <= MAX_HEIGHT);

	for (int y = 0; y < height; ++y)
		for (int x = 0; x < width + GROUP_DELAY; ++x) {
#pragma HLS PIPELINE
			if (x >= width)
				continue;

			const IN val = in_s.read();

			out_s << filter(val);
		}
}

template<int KERNEL_SIZE, typename IN, typename OUT, class Filter>
void processPixels2(
		hls::stream<IN> &in1_s,
		hls::stream<IN> &in2_s,
		hls::stream<OUT> &out_s,
		const int &width,
		const int &height,
		Filter &filter)
{
	assert(width <= MAX_WIDTH); assert(height <= MAX_HEIGHT);

	for (int y = 0; y < height; ++y)
		for (int x = 0; x < width + GROUP_DELAY; ++x) {
#pragma HLS PIPELINE
			if (x >= width)
				continue;

			const IN val1 = in1_s.read();
			const IN val2 = in2_s.read();

			out_s.write(filter(val1, val2));
		}
}

template<int KERNEL_SIZE, typename IN, typename OUT, class Filter>
void processPixels3(
		hls::stream<IN> &in1_s,
		hls::stream<IN> &in2_s,
		hls::stream<IN> &in3_s,
		hls::stream<OUT> &out_s,
		const int &width,
		const int &height,
		Filter &filter)
{
	assert(width <= MAX_WIDTH); assert(height <= MAX_HEIGHT);

	for (int y = 0; y < height; ++y)
		for (int x = 0; x < width + GROUP_DELAY; ++x) {
#pragma HLS PIPELINE
			if (x >= width)
				continue;

			const IN val1 = in1_s.read();
			const IN val2 = in2_s.read();
			const IN val3 = in3_s.read();

			out_s.write(filter(val1, val2, val3));
		}
}

/**
 * Clones a single input stream into two identical streams.
 * @param in_s The input stream.
 * @param out1_s First output stream.
 * @param out2_s Second output stream.
 */
template<int KERNEL_SIZE, typename IN, typename OUT1, typename OUT2>
void splitStream(
		hls::stream<IN> &in_s,
		hls::stream<OUT1> &out1_s,
		hls::stream<OUT2> &out2_s,
		const int &width,
		const int &height)
{
	assert(width <= MAX_WIDTH); assert(height <= MAX_HEIGHT);

	for (int y = 0; y < height; ++y)
		for (int x = 0; x < width + GROUP_DELAY; ++x) {
#pragma HLS PIPELINE
			if (x >= width)
				continue;

			const IN val = in_s.read();

			out1_s << val;
			out2_s << val;
		}
}

/**
 * Clones a single input stream into two identical streams.
 * @param in_s The input stream.
 * @param out1_s First output stream.
 * @param out2_s Second output stream.
 * @param out3_s Third output stream.
 */
//template<int KERNEL_SIZE, class Filter, typename T>
template<int KERNEL_SIZE, typename IN, typename OUT, class Filter>
void splitStream3(
		hls::stream<IN> &in_s,
		hls::stream<OUT> &out1_s,
		hls::stream<OUT> &out2_s,
		hls::stream<OUT> &out3_s,
		const int &width,
		const int &height)
{
	assert(width <= MAX_WIDTH); assert(height <= MAX_HEIGHT);

	for (int y = 0; y < height; ++y)
		for (int x = 0; x < width + GROUP_DELAY; ++x) {
#pragma HLS PIPELINE
			if (x >= width)
				continue;

			const IN val = in_s.read();

			out1_s << val;
			out2_s << val;
			out3_s << val;
		}
}

/**
 * Clones a single input stream into two identical streams.
 * @param in_s The input stream.
 * @param out1_s First output stream.
 * @param out2_s Second output stream.
 * @param out3_s Third output stream.
 * @param out4_s Fourth output stream.
 */
//template<int WIDTH, int HEIGHT, typename T>
template<int KERNEL_SIZE, typename IN, typename OUT, class Filter>
void splitStream4(
		hls::stream<IN> &in_s,
		hls::stream<OUT> &out1_s,
		hls::stream<OUT> &out2_s,
		hls::stream<OUT> &out3_s,
		hls::stream<OUT> &out4_s,
		const int &width,
		const int &height)
{
	assert(width <= MAX_WIDTH); assert(height <= MAX_HEIGHT);

	for (int y = 0; y < height; ++y)
		for (int x = 0; x < width + GROUP_DELAY; ++x) {
#pragma HLS PIPELINE
			if (x >= width)
				continue;

			const IN val = in_s.read();

			out1_s << val;
			out2_s << val;
			out3_s << val;
			out4_s << val;
		}
}

template<typename T>
struct OpOffset
{
	const T off;
	OpOffset(const T _off) : off(_off) {}
	T operator()(const T val) const { return val + off; }
};

template<typename T>
struct OpScale
{
	const T fact;
	OpScale(const T _fact) : fact(_fact) {}
	T operator()(const T val) const { return fact * val; }
};
