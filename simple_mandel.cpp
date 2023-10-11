#include <cmath>
#include <cstdio>
#include <cstdint>

#include <type_traits>

class color;
class pixel;
class complex;

class camera;
class accumulator;
class painter;

template <class accumulator_t, class painter_t>
class renderer;

class color {
public:
	uint8_t r;
	uint8_t g;
	uint8_t b;
	
	color() : r(0), g(0), b(0) {}
	
	color(uint8_t red, uint8_t green, uint8_t blue) : r(red), g(green), b(blue) {}
};

class cubic {
public:
	// ax^3 + bx^2 + cx + d
	float a;
	float b;
	float c;
	float d;
	
	cubic() : a(0), b(0), c(0), d(0) {}
	
	cubic(float a, float b, float c, float d) : a(a), b(b), c(c), d(d) {}
	
	cubic derivative() {
		return cubic(0, 3*a, 2*b, c);
	}
	
	float evaluate(float x) {
		return a*x*x*x + b*x*x + c*x + d;
	}
};

class palette_point {
public:
	color col; // The color assigned to this point.
	float pos; // The position of this point.
	
	// Equations representing how each channel changes from this point to the next point.
	cubic r;
	cubic g;
	cubic b;
	
	palette_point() {}
	
	palette_point(color point_col, float point_pos) : col(point_col), pos(point_pos), r(0, 0, 0, 0), g(0, 0, 0, 0), b(0, 0, 0, 0) {}
};

class palette {
public:
	enum interpolation {LINEAR, CUBIC};
	
	int max_points;
	int num_points;
	
	palette_point* points;
	
	interpolation inter;
	
	palette(int max_points, interpolation inter) : max_points(max_points), inter(inter), num_points(0) {
		points = new palette_point[max_points];
	}
	
	// Adds a new point.
	void add_point(const palette_point& new_point) {
		points[num_points] = new_point;
		num_points++;
	}
	
	// Performs calculations required for evaluation. Must be called prior to calling evaluate().
	void cache() {
		switch(inter) {
			// Construct equations so that the channels of the colors are interpreted linearly.
			case LINEAR:
				for (int i = 0; i < num_points-1; i++) {
					palette_point& c = points[i];
					palette_point& n = points[i+1];
					float run = n.pos - c.pos;
					
					c.r.a = 0;
					c.r.b = 0;
					c.r.c = (n.col.r - c.col.r) / run;
					c.r.d = c.col.r - c.r.c * c.pos;
					
					c.g.a = 0;
					c.g.b = 0;
					c.g.c = (n.col.g - c.col.g) / run;
					c.g.d = c.col.g - c.g.c * c.pos;
					
					c.b.a = 0;
					c.b.b = 0;
					c.b.c = (n.col.b - c.col.b) / run;
					c.b.d = c.col.b - c.b.c * c.pos;
				}
				break;
			
			// Construct equations so that the channels of the colors are interpreted cubically.
			case CUBIC:
				break;
		}
	}
	
	color evaluate(float pos) {
		if (num_points == 0) {
			return color(255, 105, 180); // Hot pink.
		}
		
		if (pos <= points[0].pos) {
			return points[0].col;
		}
		else if (pos > points[num_points-1].pos) {
			return points[num_points-1].col;
		}
		
		for (int i = 1; i < num_points; i++) {
			if (points[i].pos > pos) {
				return color(
					points[i-1].r.evaluate(pos),
					points[i-1].g.evaluate(pos),
					points[i-1].b.evaluate(pos)
				);
			}
		}
		
		printf("Palette has reached an invalid state.\n");
		exit(0);
		return color(0, 0, 0); // Warning suppression.
	}
	
	void sample(const char* filename, size_t width, size_t height) {
		float min_pos = points[0].pos;
		float max_pos = points[num_points-1].pos;
		float range = max_pos - min_pos;
		
		// Create the image.
		uint8_t* img = new uint8_t[width*height*3];
		for (int x = 0; x < width; x++) {
			color col = evaluate((double) x / width * range + min_pos);
			
			for (int y = 0; y < height; y++) {
				int index = (y*width + x)*3;
				img[index  ] = col.r;
				img[index+1] = col.g;
				img[index+2] = col.b;
			}
		}
		
		// Save the image.
		char hdr[128];
		int hdr_len = sprintf(hdr, "P6 %ul %ul 255 ", width, height);
		
		FILE* fout = fopen(filename, "wb");
		fwrite(hdr, 1, hdr_len, fout);
		fwrite(img, 1, width*height*3, fout);
		fclose(fout);
		
		delete[] img;
	}
	
	~palette() {
		delete[] points;
	}
};

class pixel {
public:
	unsigned int x;
	unsigned int y;
	
	pixel(unsigned int pixel_x, unsigned int pixel_y) : x(pixel_x), y(pixel_y) {}
};

// Represents a complex number with a real component and an imaginary component.
class complex {
public:
	double r;
	double i;
	
	complex() : r(0), i(0) {}
	
	complex(double real, double imaginary) : r(real), i(imaginary) {}
	
	// Add the addend to this one.
	void add(const complex& addend) {
		r += addend.r;
		i += addend.i;
	}
	
	// Multiply this number by the multiplicand.
	void multiply(const complex& multiplicand) {
		double temp = r*multiplicand.r - i*multiplicand.i;
		i = r*multiplicand.i + i*multiplicand.r;
		r = temp;
	}
	
	// Squares this complex number and then adds the passed addend to it.
	void square_and_add(const complex& addend) {
		double temp = r*r - i*i + addend.r;
		i = 2*r*i + addend.i;
		r = temp;
	}
	
	// Retrieve the square of the absolute value of this number.
	float sqr_abs() const {
		return r*r + i*i;
	}
};

// A 2D transformation matrix.
class camera {
public:
	// New x and y axis, plus offset.
	complex x;
	complex y;
	complex o;
	
	// Given the width and height of the output image,
	// and the coordinates of the lower-left and upper-right corners of the region of the complex plane to render,
	// construct a transformations matrix with only scaling and translation.
	camera(size_t width, size_t height, double left, double bottom, double right, double top) :
		x( (right - left) / width, 0 ),
		y( 0, (top - bottom) / height ),
		o( left, bottom )
	{}
	
	// Applies the matrix to the given coordinates, converting from pixel coordinates to complex coordinates.
	complex pixel_to_complex(pixel pos) const {
		return complex(
			x.r * pos.x + y.r * pos.y + o.r,
			x.i * pos.x + y.i * pos.y + o.i
		);
	}
	
	// Translates the matrix and viewport by the passed complex number.
	void translate(const complex& offset) {
		o.add(offset);
	}
	
	// Rotates the matrix - and thus the viewport - clockwise by the passed number of radians.
	void rotate(float rad) {
		complex rot(cos(rad), sin(rad));
		
		x.multiply(rot);
		y.multiply(rot);
	}
	
	// Print the matrix
	void debug() {
		printf(" X      ,  Y      , O\n");
		printf("%8.5lf, %8.5lf, %8.5lf\n", x.r, y.r, o.r);
		printf("%8.5lf, %8.5lf, %8.5lf\n", x.i, y.i, o.i);
	}
};

// Type from which all accumulators must derive.
class accumulator {
public:
	unsigned int n;
	renderer<accumulator, painter>* rend;
	
	accumulator() {}
};

// Type from which all painters must derive.
class painter {
public:
	renderer<accumulator, painter>* rend;
	
	painter() {}
};

template <class accumulator_t, class painter_t>
class renderer {
public:
	// The width and height of the output image.
	const size_t width;
	const size_t height;
	
	// A camera that maps pixel coordinates to complex numbers.
	camera cam;
	
	// An array of pixel values.
	uint8_t* img;
	
	// The Mandelbrot accumulators.
	accumulator_t* accum;
	
	// A class that implements fraactal_painter
	painter_t* paint;
	
	renderer(size_t grid_width, size_t grid_height, const camera& rend_cam, painter_t* rend_painter) :
		width(grid_width), height(grid_height), cam(rend_cam), paint(rend_painter) {
		
		static_assert(std::is_base_of_v<accumulator, accumulator_t>, "First template parameter must derive from accumulator.");
		static_assert(std::is_base_of_v<painter, painter_t>, "Second template parameter must derive from painter.");
		
		img = new uint8_t[width*height*3];
		
		// Initialize painter.
		paint->rend = (renderer<accumulator, painter>*) this;
		
		// Initialize accumulators.
		accum = new accumulator_t[width*height];
		for (int y = 0; y < height; y++) {
			for (int x = 0; x < width; x++) {
				accum[y*width + x] = accumulator_t(cam, pixel(x, y));
				accum[y*width + x].n = 0;
				accum[y*width + x].rend = (renderer<accumulator, painter>*) this;
			}
		}
	}
	
	void iterate_all(unsigned int num_n) {
		for (int i = 0; i < width*height; i++) {
			accum[i].iterate(num_n);
		}
	}
	
	void paint_all() {
		for (int x = 0; x < width; x++) {
			for (int y = 0; y < height; y++) {
				int index = y*width + x;
				
				color col = paint->get_color_of(accum[index], pixel(x, y));
				
				img[index*3  ] = col.r;
				img[index*3+1] = col.g;
				img[index*3+2] = col.b;
			}
		}
	}
	
	void save(const char* filename) {
		char hdr[64];
		int hdr_len = sprintf(hdr, "P6 %ul %ul 255 ", width, height);
		
		FILE* fout = fopen(filename, "wb");
		fwrite(hdr, 1, hdr_len, fout);
		fwrite(img, 1, width*height*3, fout);
		fclose(fout);
	}
	
	~renderer() {
		delete[] img;
		delete[] accum;
	}
};

/* ---- Custom classes & functions ---- */

class mandel_accumulator : public accumulator {
public:
	complex C;
	complex Z;
	
	mandel_accumulator() {}
	
	mandel_accumulator(const camera& cam, const pixel& pos) {
		C = cam.pixel_to_complex(pos);
		Z = complex(0, 0);
	}
	
	void iterate(unsigned int num_n) {
		if (!is_inside_fractal()) return;
		
		int i;
		for (i = 0; i < num_n; i++) {
			Z.square_and_add(C);
			
			if (!is_inside_fractal()) {
				i++;
				break;
			}
		}
		
		n += i;
	}
	
	bool is_inside_fractal() const {
		return Z.sqr_abs() < 400;
	}
};

class mandel_binary_painter : public painter {
public:
	color in;
	color out;
	
	mandel_binary_painter(color inside, color outside) : in(inside), out(outside) {}
	
	virtual color get_color_of(const mandel_accumulator& calc, const pixel& pos) {
		if (calc.is_inside_fractal()) {
			return in;
		}
		else {
			return out;
		}
	}
};

class mandel_renormalized_painter : public painter {
public:
	color in;
	
	mandel_renormalized_painter(color inside) : in(inside) {}
	
	color get_color_of(const mandel_accumulator& calc, const pixel& pos) {
		if (calc.is_inside_fractal()) {
			return in;
		}
		else {
			float val = sqrt(calc.Z.sqr_abs());
			val = calc.n + 1 - log(log(val))/log(2);		
			val = val / 50 * 255;
			return color(val, val, val);
		}
	}
};

int main() {
	// Size of the image to produce
	const size_t width = 1920;
	const size_t height = 1080;
	
	// The maximum number of iterations to perform.
	int max_n = 50;
	
	// The region we'd like to render
	double left = -2.6;
	double bottom = -1.181;
	double right = 1.6;
	double top = 1.181;
	
	palette pal(3, palette::LINEAR);
	pal.add_point( palette_point(color(180, 20, 60), 0) );
	pal.add_point( palette_point(color(10, 20, 200), 0.3) );
	pal.add_point( palette_point(color(110, 20, 200), 1.0) );
	pal.cache();
	pal.sample("sample.ppm", 800, 200);
	
	// Create a camera
	camera cam(width, height, left, bottom, right, top);
	
	mandel_renormalized_painter painter(color(0, 0, 0));
	
	renderer<mandel_accumulator, mandel_renormalized_painter> rend(width, height, cam, &painter);
	
	rend.iterate_all(max_n);
	rend.paint_all();
	rend.save("out.ppm");
	
	return 0;
}
