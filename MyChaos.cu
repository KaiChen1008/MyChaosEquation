#include <iostream>
#include <random>
#include <sstream>
#include <cassert>
#include <fstream>
#include <cfloat>
#include <cstdlib>
#include <iostream>
#include <string>
#include <ctime>
#include <pthread.h>

// #include <png.h>


#include <cuda.h>
#include <cuda_runtime.h>

using namespace std;

//Global constants
static const int num_params = 18;
static const int iters = 800;
static const int steps_per_frame = 500;
static const double delta_per_step = 1e-5;
static const double delta_minimum = 1e-7;
static const double t_start = -3.0;
static const double t_end = 3.0;
static const int fad_speed = 10;
static std::mt19937 rand_gen;
static const float dot_sizes[3] = { 1.0f, 3.0f, 10.0f };

//Global variables
static int window_w = 1600;
static int window_h = 900;
static int window_bits = 24;
static float plot_scale = 0.25f;
static float plot_x = 0.0f;
static float plot_y = 0.0f;


double params[num_params];              // 18 
int start_point[6] = {0, 1, 2, 3, 4, 5};


pthread_mutex_t t_mutex   = PTHREAD_MUTEX_INITIALIZER;
int num_threads = 6;

struct Color{
    int r;
    int g;
    int b;
};

struct Vector2f{
    double x;
    double y;
} ;

struct Vertex{
    Vector2f position;
    Color  color;
};



static Color GetRandColor(int i) {
  i += 1;
  int r = std::min(255, 50 + (i * 11909) % 256);
  int g = std::min(255, 50 + (i * 52973) % 256);
  int b = std::min(255, 50 + (i * 44111) % 256);
  return Color{r, g, b};
}

static void ResetPlot() {
  plot_scale = 0.25f;
  plot_x = 0.0f;
  plot_y = 0.0f;
}

static Vector2f ToScreen(double x, double y) {
  const float s = plot_scale * float(window_h / 2);
  const float nx = float(window_w) * 0.5f + (float(x) - plot_x) * s;
  const float ny = float(window_h) * 0.5f + (float(y) - plot_y) * s;
  return Vector2f{nx, ny};
}

static void RandParams(double* params) {
	params[ 0] = 1; params[ 1] = 0; params[ 2] = 0;
	params[ 3] = 0; params[ 4] =-1; params[ 5] = 1;
	params[ 6] =-1; params[ 7] = 0; params[ 8] = 0;

	params[ 9] = 0; params[10] =-1; params[11] =-1;
	params[12] =-1; params[13] =-1; params[14] =-1;
	params[15] = 0; params[16] =-1; params[17] = 0;
}

static void draw_png() {
    // write_png(output);
	// dot_size
	// window.draw(vertex_array.data(), vertex_array.size(), sf::PrimitiveType::Points);
}

void* thread_target(void* arg) {
    int* start = (int*) arg;
    double t =  double(start[0]);
    
    // Setup the vertex array
    vector<Vertex> vertex_array(iters * steps_per_frame); // 800 * 500
    for (size_t i = 0; i < vertex_array.size(); ++i) 
        vertex_array[i].color = GetRandColor(i % iters);


    for (; t < t+1.0; )
    {
        for (int step = 0; step < steps_per_frame; ++step) //steps = 500
        {
			bool isOffScreen = true;
            double x = t;
            double y = t;
            for (int iter = 0; iter < iters; ++iter) // 800
            {
                const double xx = x * x; const double yy = y * y; const double tt = t * t;
                const double xy = x * y; const double xt = x * t; const double yt = y * t;

                const double nx = xx * params[ 0] + yy * params[ 1] + tt * params[ 2] + 
                            xy * params[ 3] + xt * params[ 4] + yt * params[ 5] + 
                            x  * params[ 6] + y  * params[ 7] + t  * params[ 8] ;
                
                const double ny = xx * params[ 9] + yy * params[10] + tt * params[11] + 
                            xy * params[12] + xt * params[13] + yt * params[14] + 
                            x  * params[15] + y  * params[16] + t  * params[17] ;

                x = nx;
                y = ny;

				Vector2f screenPt = ToScreen(x,y);
                if (iter < 100)
                {
                    screenPt.x = FLT_MAX;
                    screenPt.y = FLT_MAX;
                }

                vertex_array[step*iters + iter].position = screenPt;


            } //iteration end

			t += 1e-7;
        } // step end


		// Draw the data
		draw_png();

    } // t end

}

int main(int argc, char* argv[]) {
	clock_t start, stop;
	cout << "start computing........." << endl;
	start = clock();


    // Initialize random parameters
    ResetPlot();
    RandParams(params);

    
    pthread_t threads[num_threads];

    for (int i = 0; i < num_threads; ++i)
    {
        assert (0 == pthread_create(&threads[i], NULL, thread_target, (void*) &start_point[i]);
    }
        

    for (int i = 0; i< num_threads; ++i)
    {
        assert(0 == pthread_join(threads[i], NULL));
    }

	stop = clock();
	cout << double(stop - start) / CLOCKS_PER_SEC << endl;
    return 0;
}



