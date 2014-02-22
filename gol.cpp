#include <GL/glfw.h>
#include "scopeguard.hpp"
#include <iostream>
#include <unordered_map>
#include <string>
#include <vector>
#include <random>
#include <chrono>
#include <limits>
#include <cmath>
#include <cstdint>

using namespace std;

unsigned const Width = 640,
               Height = 480;

string const Title = "Game of Life";

typedef unsigned char uchar;
typedef uint16_t state_chart_t[2][2][2][2][2][2][2][2][2][2][2][2];
typedef uchar world_t[Height][Width];

void init_scene(GLuint&);
void destroy_scene(GLuint&);
template <typename T>
void draw_scene(GLuint, T const&, bool const);
template <typename T>
void update_texture(GLuint&, T const&);

void create_state_chart(state_chart_t);

void create_world(world_t);
void update_world(world_t const /*cur*/, world_t/*prev*/, state_chart_t const);

size_t hash_fnv(world_t const &world)
{
    size_t res = numeric_limits<size_t>::digits == 64 ? 14695981039346656037UL : 2166136261UL;
    auto const prime = numeric_limits<size_t>::digits == 64 ? 1099511628211UL : 16777619UL;
    static_assert(Width%8 == 0, "must be multiple of 8");
    for(auto y = 0; y < Height; y++)
        for(auto x = 0; x < Width; x += 8)
        {
            uint8_t octet = (world[y][x] & 0x80) |
                (world[y][x+1] & 0x40) |
                (world[y][x+2] & 0x20) |
                (world[y][x+3] & 0x10) |
                (world[y][x+4] & 0x08) |
                (world[y][x+5] & 0x04) |
                (world[y][x+6] & 0x02) |
                (world[y][x+7] & 0x01);
            res ^= octet;
            res *= prime;
        }
    return res;
}

struct timer
{
    chrono::steady_clock::time_point start, end;
    timer(double expire=0) :
        start(chrono::steady_clock::now()), end(start + chrono::milliseconds(unsigned(expire*1000))) {}
    bool expired() { return chrono::steady_clock::now() >= end; }
    void reset(string header)
    {
        return;
        auto now = chrono::steady_clock::now();
        cout << header << ' ' << chrono::duration_cast<chrono::microseconds>(now - start).count() << " us\n";
        start = now;
    }
};

int main()
{
    if(!glfwInit())
    {
        cerr << "glfwInit failed\n";
        return 1;
    }
    DEFER { glfwTerminate(); };

    glfwOpenWindowHint(GLFW_WINDOW_NO_RESIZE, true);

    if(!glfwOpenWindow(Width, Height, 0, 0, 0, 0, 16, 0, GLFW_WINDOW))
    {
        cerr << "glfwOpenWindow failed\n";
        return 1;
    }
    DEFER { glfwCloseWindow(); };

    glfwSwapInterval(1);
    glfwSetWindowTitle(Title.c_str());

    GLuint texture = 0;
    init_scene(texture);
    DEFER { destroy_scene(texture); };

    state_chart_t state_chart;
    create_state_chart(state_chart);
    world_t world[2];
    create_world(world[0]);
    unsigned long frames = 0,
                  updates = 0;
    unordered_map<size_t, unsigned long> history;
    bool cyclic = false;
    vector<vector<uchar>> cycle;
    unsigned cycle_length = 0;
    unsigned cycles_initialized = 0;
    timer reset_timer;
    while(glfwGetWindowParam(GLFW_OPENED))
    {
        if(cyclic && reset_timer.expired())
        {
            cyclic = false;
            frames = 0;
            updates = 0;
            create_world(world[0]);
            history.clear();
            cycle.clear();
        }
        timer t;
        if(frames++ > 0)
        {
            if(cyclic)
            {
               if(cycles_initialized != cycle_length)
               {
                   auto cur = cycle[cycles_initialized++];
                   cur.insert(cur.begin(), &world[updates%2][0][0], &world[updates%2][0][0]+(Height*Width));
               }
            }
            updates++;
            if(!cyclic || cycles_initialized != cycle_length)
                update_world(world[updates%2], world[(updates-1)%2], state_chart);
        }
        t.reset("update world");
        size_t hash = hash_fnv(world[updates%2]);
        t.reset("hash");
        auto pos = history.find(hash);
        if(pos == history.end())
            history[hash] = updates;
        else
        {
            if(!cyclic)
            {
                cyclic = true;
                cout << "World repeats in " << (cycle_length = updates - pos->second) << " steps after "
                     << history.size() << " generations\n";
                cycle.resize(cycle_length);
                cycles_initialized = 0;
                reset_timer = timer(max(5., sqrt(cycle_length)));
            }
        }
        t.reset("foo");
        if(cyclic && cycles_initialized == cycle_length)
            update_texture(texture, &(cycle[updates%cycle_length].front()));
        else
            update_texture(texture, world[updates%2]);
        t.reset("texture");
        draw_scene(texture, world, cyclic);
        t.reset("draw");
        glfwSwapBuffers();
        t.reset("swap");
    }
}

void create_state_chart(state_chart_t chart)
{
    /*  1   2   3   4
     *  5   l   r   6
     *  7   8   9   10
     */
    union
    {
        unsigned val;
        struct { unsigned b0:1,b1:1,b2:1,b3:1,b4:1,b5:1,b6:1,b7:1,b8:1,b9:1,b10:1,b11:1,b12:1; } b1;
    } i;
    for(i.val = 0; i.val < (1<<12); i.val++)
    {
        const auto sum = [](unsigned i) -> unsigned {
            auto j = i - ((i>>1)&0x55555555);
            j = (j & 0x33333333) + ((j >> 2) & 0x33333333);
            return (((j + (j >> 4)) & 0x0F0F0F0F) * 0x01010101) >> 24;
        };
        const auto l = [](unsigned s, unsigned l) -> uchar {
            return l ? ((s == 2 || s == 3) ? 255 : 0) : (s == 3 ? 255 : 0);
        };
        //fmt.Printf("%v -%v-> %v\n", i, (i>>8)&1, sum)
        const auto left_mask = 0x757,
                   right_mask = 0xeae;
        chart[i.b1.b11][i.b1.b10][i.b1.b9][i.b1.b8][i.b1.b7][i.b1.b6][i.b1.b5][i.b1.b4][i.b1.b3][i.b1.b2][i.b1.b1][i.b1.b0] = l(sum(i.val&right_mask), i.b1.b6) | (l(sum(i.val&left_mask), i.b1.b5)<<8);
    }
}

void create_world(world_t world)
{
    auto seed = random_device()();
    cout << "Seed: " << seed << '\n';
    mt19937 rand(seed);
    for(auto y = 0; y < Height; y++)
        for(auto x = 0; x < Width; x++)
            world[y][x] = ((rand()%10) == 0)*255;
}

void update_world(world_t const cur, world_t prev, state_chart_t const chart)
{
    for(auto y = 0; y < Height; y++)
        for(auto x = 0; x < Width; x+=2)
        {
            const auto h = [&](int h) -> unsigned {
                return (Height+y+h)%Height;
            };
            const auto w = [&](int w) -> unsigned {
                return (Width+x+w)%Width;
            };
            *((uint16_t*)&cur[y][x]) = chart[prev[h(-1)][w(-1)]&1][prev[h(-1)][x]&1][prev[h(-1)][w(1)]&1][prev[h(-1)][w(2)]&1][prev[y][w(-1)]&1][prev[y][x]&1][prev[y][w(1)]&1][prev[y][w(2)]&1][prev[h(1)][w(-1)]&1][prev[h(1)][x]&1][prev[h(1)][w(1)]&1][prev[h(1)][w(2)]&1];
        }
}

template <typename T>
void update_texture(GLuint &tex, T const &world)
{
    //glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, Width, Height, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, world);
}

void init_texture(GLuint &tex)
{
    /*glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);*/
}

void init_scene(GLuint &tex)
{
    //glEnable(GL_TEXTURE_2D);
    glClearColor(1, 1, 1, 1);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(1, Width, 1, Height, 0, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    init_texture(tex);
}

void destroy_scene(GLuint &tex)
{
    //glDeleteTextures(1, &tex);
}

template <typename T>
void draw_scene(GLuint tex, T const &world, bool const cyclic)
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    //glMatrixMode(GL_MODELVIEW);
    //glLoadIdentity();
    glDrawPixels(Width, Height, cyclic ? GL_GREEN : GL_LUMINANCE, GL_UNSIGNED_BYTE, world);
    /*glBindTexture(GL_TEXTURE_2D, tex);
    glColor4f(1, 1, 1, 1);

    glBegin(GL_QUADS);
    glTexCoord2f(0, 0);
    glVertex2f(1, 1);
    glTexCoord2f(0, 1);
    glVertex2f(1, Height);
    glTexCoord2f(1, 1);
    glVertex2f(Width, Height);
    glTexCoord2f(1, 0);
    glVertex3f(Width, 1, 0);
    glEnd();*/

}


