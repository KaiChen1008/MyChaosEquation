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
#include <queue>
#include <pthread.h>
#include <png.h>
#include "tbb/concurrent_queue.h"
#include <chrono> 
// #include <cuda.h>
// #include <cuda_runtime.h>

using namespace std;
using namespace tbb;

clock_t start, stop;
//Global constants
#define t_step 1e-7
#define queue_size 25
static const int iters = 800;
static const int steps_per_frame = 2000;
static const double delta_per_step = 1e-5;
static const double delta_minimum = 1e-7;
static const double t_start = -3.0;
static const double t_end = 3.0;
static const int fad_speed = 10;
static std::mt19937 rand_gen;
static const float dot_sizes[3] = { 1.0f, 3.0f, 10.0f };
static const int num_params = 18;
double params[num_params];              // 18 

//Global variables
static int window_w = 1600;
static int window_h = 900;
static int window_bits = 24;
static float plot_scale = 0.25f;
static float plot_x = 0.0f;
static float plot_y = 0.0f;

// thread constants

int num_computing_threads = 2;
int num_io_threads = 6;

// int start_points[6] = {0, 1, 2, 3, 4, 5};
// int start_points[3] = {0, 2, 4}; // start from [i] - 3;
int start_points[2] = {0, 3};
int each_thread_step = 3;//int(6/num_computing_threads); // -3 ~ 3 / num_computing_therads

int io_point[6] = {0, 1, 2, 3, 4, 5};

int computing_to_io_ratio = 3; // 1 computing thread maps to 3 io thread


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

struct V{
    vector<Vertex> vertex_array; // 800 * 500
    double t;
};

// queue<V> vertex_array_queue[6];
concurrent_queue<V> vertex_array_queue[6];

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

void write_png(const char* filename, const int width, const int height, const int* imageR, const int* imageG, const int* imageB) {
    FILE* fp = fopen(filename, "wb");
    assert(fp);

    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL); 	assert(png_ptr);
    png_infop info_ptr = png_create_info_struct(png_ptr);										assert(info_ptr);

    png_init_io(png_ptr, fp);
    png_set_IHDR(png_ptr, info_ptr, width, height, 8, PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    
    png_write_info(png_ptr, info_ptr);
    // png_set_compression_level(png_ptr, 0);

    size_t row_size = 3 * width * sizeof(png_byte);

    png_bytep row = (png_bytep)malloc(row_size);

    for (int y = 0; y < height; ++y) {
        memset(row, 0, row_size);
        for (int x = 0; x < width; ++x) {
            png_bytep color = row + x * 3;
			color[0] = imageR[x + y * window_w];
			color[1] = imageG[x + y * window_w];
			color[2] = imageB[x + y * window_w];
        }
        png_write_row(png_ptr, row);
    }


    free(row);
    png_write_end(png_ptr, NULL);
    png_destroy_write_struct(&png_ptr, &info_ptr);
    fclose(fp);
}

void create_png(vector<Vertex>& vertex_array, double t) {

	// allocate memory for image 
	size_t image_size = window_w * window_h * sizeof(int);
    int* imageR = (int*)malloc(image_size);
	int* imageG = (int*)malloc(image_size);
	int* imageB = (int*)malloc(image_size);
	memset(imageR, 0, image_size);
	memset(imageG, 0, image_size);
	memset(imageB, 0, image_size);

    // plot the points  
    for (size_t i = 0; i < vertex_array.size(); ++i) 
    {
        Vector2f screenPt = vertex_array[i].position; // double
        Color    color    = vertex_array[i].color; // int
		int x = int(screenPt.x);
		int y = int(screenPt.y);

        if (screenPt.x > 0.0f && screenPt.y > 0.0f && screenPt.x < window_w && screenPt.y < window_h)
        {
			imageR[x + y * window_w] = abs(imageR[x + y * window_w] - color.r);
			imageG[x + y * window_w] = abs(imageG[x + y * window_w] - color.g);
			imageB[x + y * window_w] = abs(imageB[x + y * window_w] - color.b);
        }
    }

    // start I/O
	double file_name_double = (t + 3.0)/t_step;
	// cout << "filename: " << t << " ";

	char filename[30];
	// sprintf(filename , "./pic/%06d.png" , int(file_name_double));
	sprintf(filename, "./pic/%09d.png", int(file_name_double));

	//cout << filename << endl;
	write_png(filename, window_w, window_h, imageR, imageG, imageB);
	free(imageR);
	free(imageG);
	free(imageB);
}

void* thread_target(void* arg) {
    int* start = (int*) arg;
    int thread_num = int(start[0]);
    int which_io = 0;
    int full_hits = 0;
    // timestep setting
    double t =  double(thread_num) - 3;
    double local_t_end = t + each_thread_step;

    // Setup the vertex array
    V result;
    result.vertex_array.resize(iters * steps_per_frame); // 800 * 500
    // vector<Vertex> vertex_array(iters * steps_per_frame);
    
    for (size_t i = 0; i < result.vertex_array.size(); ++i) 
        result.vertex_array[i].color = GetRandColor(i % iters);


    while(t < local_t_end)
    {
        // wait for i/o 
        if (vertex_array_queue[thread_num + which_io].unsafe_size() >= queue_size)
        {

            // cout << "full hits: " << ++full_hits << " ,which io thread: " << thread_num + which_io << endl;
            continue;
        }else 
        {
            full_hits = 0;
        }

        for (int step = 0; step < steps_per_frame; ++step) //steps = 2000
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

                result.vertex_array[step*iters + iter].position = screenPt;

            } //iteration end

			t += t_step;
        } // step end

		// Draw the data
        // put the vertex array to queue
        result.t = t;
        vertex_array_queue[thread_num + which_io].push(result);
        which_io = (which_io + 1) % computing_to_io_ratio;

    } // t end

    cout << "computing thread: " << thread_num << " finished" << endl;
    result.t = -100;
    for (int i = 0; i < computing_to_io_ratio; i++)
        vertex_array_queue[thread_num + i].push(result);
    
    pthread_exit(NULL);
}

void* thread_io_target(void* arg) {
    int* start = (int*) arg;
    int io_num =  int(start[0]);
    int empty_hits = 0;
    // cout << "io thread: " << io_num << " start working" << endl;
    while (true)
    {
        if (vertex_array_queue[io_num].empty())
        {
            // cout << "empty hits: " << ++empty_hits  <<", which io: " << io_num << endl;
            continue;
        }else
        {
            empty_hits = 0;
        }
        
        // take out the first result
        // V result = vertex_array_queue[io_num].front();
        // vertex_array_queue[io_num].pop();
        V result;
        if (!vertex_array_queue[io_num].try_pop(result))
            continue;
        
        // check if the computing thread finished
        if (result.t == -100)
            break;

        vector<Vertex> vertex_array;
        vertex_array.resize(result.vertex_array.size());
        vertex_array = result.vertex_array;

        double t = result.t;
        create_png(vertex_array, t);
    }

    cout << "io thread " << io_num << " exits" << endl;
    pthread_exit(NULL);
}

int main(int argc, char* argv[]) {
	cout << "start computing........." << endl;
    chrono::steady_clock::time_point t1 = chrono::steady_clock::now();
	start = clock();

    rand_gen.seed((unsigned int)time(0));
    // Initialize random parameters
    ResetPlot();
    RandParams(params);

    
    pthread_t computing_threads[num_computing_threads];
    pthread_t io_threads[num_io_threads];

    // create computing threads
    for (int i = 0; i < num_computing_threads; ++i)
        assert (0 == pthread_create(&computing_threads[i], NULL, thread_target, (void*) &start_points[i]));

    // create i/o threads
    for (int i = 0; i < num_io_threads; ++i)
        assert (0 == pthread_create(&io_threads[i], NULL, thread_io_target, (void*) &io_point[i]));


    // join computing threads
    for (int i = 0; i < num_computing_threads; ++i)
        assert(0 == pthread_join(computing_threads[i], NULL));
    
    // join computing threads
    for (int i = 0; i < num_io_threads; ++i)
        assert(0 == pthread_join(io_threads[i], NULL));
    

	stop = clock();
    chrono::steady_clock::time_point t2 = chrono::steady_clock::now();
	cout << double(stop - start) / CLOCKS_PER_SEC << endl;
    cout <<"total time: " << chrono::duration_cast<chrono::microseconds>(t2 - t1).count() << " us" << endl;
    return 0;
}