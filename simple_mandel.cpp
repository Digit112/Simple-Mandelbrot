#include <cmath>
#include <cstdio>
#include <cstdint>

#include <type_traits>

class color {
public:
	uint8_t r;
	uint8_t g;
	uint8_t b;
	
	color(uint8_t red, uint8_t green, uint8_t blue) : r(red), g(green), b(blue) {}
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

// Base class for a fractal calculator.
// Derivatives of this class may render different fractals.
class fractal_calculator {
public:
	unsigned int n;
	
	fractal_calculator() : n(0) {}
	
	// Iterate the equation num_n times. The function is responsible for the maintenance of the fractal_calculator::n member.
	virtual void iterate(unsigned int num_n) = 0;
	
	// Returns true if this point is beleived to be inside the fractal, false otherwise.
	virtual bool is_inside_fractal() const = 0;
};

class fractal_painter {
public:
	virtual color get_color_of(const fractal_calculator&, const pixel& pos) = 0;
};

template <class T>
class fractal_renderer {
public:
	// The width and height of the output image.
	const size_t width;
	const size_t height;
	
	// A camera that maps pixel coordinates to complex numbers.
	camera cam;
	
	// An array of pixel values.
	uint8_t* img;
	
	// A class that implements fractal_calculator
	T* calculators;
	
	// A class that implements fraactal_painter
	fractal_painter* painter;
	
	fractal_renderer(size_t grid_width, size_t grid_height, const camera& rend_cam, fractal_painter* rend_painter) :
		width(grid_width), height(grid_height), cam(rend_cam), painter(rend_painter) {
		
		static_assert(std::is_base_of_v<fractal_calculator, T>, "Template parameter must derive from fractal_calculator.");
		
		img = (uint8_t*) malloc(width*height*3*sizeof(uint8_t));
		
		calculators = new T[width*height];
		for (int y = 0; y < height; y++) {
			for (int x = 0; x < width; x++) {
				calculators[y*width + x] = T(cam, pixel(x, y));
			}
		}
	}
	
	void iterate_all(unsigned int num_n) {
		for (int i = 0; i < width*height; i++) {
			calculators[i].iterate(num_n);
		}
	}
	
	void paint_all() {
		for (int x = 0; x < width; x++) {
			for (int y = 0; y < height; y++) {
				int index = y*width + x;
				
				color col = painter->get_color_of(calculators[index], pixel(x, y));
				
				img[index*3  ] = col.r;
				img[index*3+1] = col.g;
				img[index*3+2] = col.b;
			}
		}
	}
	
	void save(const char* filename) {
		char hdr[64];
		int hdr_len = sprintf(hdr, "P6 %ul %ul 255 ", width, height);
		
		FILE* fout = fopen(filename, "w");
		fwrite(hdr, 1, hdr_len, fout);
		fwrite(img, 1, width*height*3, fout);
		fclose(fout);
	}
	
	~fractal_renderer() {
		free(img);
		free(calculators);
	}
};

/* ---- Custom classes & functions ---- */

class mandelbrot_calculator : public fractal_calculator {
public:
	complex C;
	complex Z;
	
	mandelbrot_calculator() {}
	
	mandelbrot_calculator(const camera& cam, const pixel& pos) {
		C = cam.pixel_to_complex(pos);
		Z = complex(0, 0);
	}
	
	virtual void iterate(unsigned int num_n) {
		if (!is_inside_fractal()) return;
		
		int i;
		for (i = 1; i <= num_n; i++) {
			Z.square_and_add(C);
			
			if (!is_inside_fractal()) break;
		}
		
		n += i;
	}
	
	virtual bool is_inside_fractal() const {
		return Z.sqr_abs() < 4;
	}
};

class mandelbrot_binary_painter : public fractal_painter {
public:
	virtual color get_color_of(const fractal_calculator& calc, const pixel& pos) {
		uint8_t col = !calc.is_inside_fractal() * 255;
		
		return color(col, col, col);
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
	
	// Create a camera
	camera cam(width, height, left, bottom, right, top);
	
	mandelbrot_binary_painter painter;
	
	fractal_renderer<mandelbrot_calculator> rend(width, height, cam, &painter);
	
	rend.iterate_all(max_n);
	rend.paint_all();
	rend.save("out.ppm");
	
	return 0;
}