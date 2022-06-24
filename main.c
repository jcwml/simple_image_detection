/*
--------------------------------------------------
    James William Fletcher (james@voxdsp.com)
        October 2021
--------------------------------------------------

    CS:GO PLATINUM
    
    Prereq:
    sudo apt install libxdo-dev libxdo3 libespeak1 libespeak-dev espeak

    Compile:
    clang csgo_gold3_fnn.c -Ofast -mavx -mfma -lX11 -lxdo -lespeak -lm -o fgold3
*/

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>
#include <X11/Xutil.h>
#include <signal.h>
#include <sys/stat.h>

#include <xdo.h>
#include <espeak/speak_lib.h>

#pragma GCC diagnostic ignored "-Wgnu-folding-constant"
#pragma GCC diagnostic ignored "-Wunused-result"

#define SCAN_DELAY 1000 // microseconds
#define ACTIVATION_SENITIVITY 0.9f

#define uint unsigned int
#define SCAN_WIDTH 28
#define SCAN_HEIGHT 28

uint REPEAT_ACTIVATION = 0;

const uint sw = SCAN_WIDTH;
const uint sh = SCAN_HEIGHT;
const uint sw2 = sw/2;
const uint sh2 = sh/2;
const uint slc = sw*sh;
const uint slall = slc*3;

float input[slall] = {0};

Display *d;
int si;
Window twin;
unsigned int x=0, y=0;
unsigned int tc = 0;

char targets_dir[256];
char nontargets_dir[256];

/***************************************************
   ~~ Utils
*/
//https://www.cl.cam.ac.uk/~mgk25/ucs/keysymdef.h
//https://stackoverflow.com/questions/18281412/check-keypress-in-c-on-linux/52801588
int key_is_pressed(Display* dpy, KeySym ks)
{
    char keys_return[32];
    XQueryKeymap(dpy, keys_return);
    KeyCode kc2 = XKeysymToKeycode(dpy, ks);
    int isPressed = !!(keys_return[kc2 >> 3] & (1 << (kc2 & 7)));
    return isPressed;
}

unsigned int espeak_fail = 0;
void speakS(const char* text)
{
    if(espeak_fail == 1)
    {
        char s[256];
        sprintf(s, "/usr/bin/espeak \"%s\"", text);
        system(s);
        usleep(33000);
    }
    else
    {
        espeak_Synth(text, strlen(text), 0, 0, 0, espeakCHARS_AUTO, NULL, NULL);
    }
}

uint qRand(const uint min, const uint max)
{
    static float rndmax = (float)RAND_MAX;
    return ( ( (((float)rand())+1e-7f) / rndmax ) * ((max+1)-min) ) + min;
}

uint64_t microtime()
{
    struct timeval tv;
    struct timezone tz;
    memset(&tz, 0, sizeof(struct timezone));
    gettimeofday(&tv, &tz);
    return 1000000 * tv.tv_sec + tv.tv_usec;
}

/***************************************************
   ~~ X11 Utils
*/

Window getWindow(Display* d, const int si) // gets child window mouse is over
{
    XEvent event;
    memset(&event, 0x00, sizeof(event));
    XQueryPointer(d, RootWindow(d, si), &event.xbutton.root, &event.xbutton.window, &event.xbutton.x_root, &event.xbutton.y_root, &event.xbutton.x, &event.xbutton.y, &event.xbutton.state);
    event.xbutton.subwindow = event.xbutton.window;
    while(event.xbutton.subwindow)
    {
        event.xbutton.window = event.xbutton.subwindow;
        XQueryPointer(d, event.xbutton.window, &event.xbutton.root, &event.xbutton.subwindow, &event.xbutton.x_root, &event.xbutton.y_root, &event.xbutton.x, &event.xbutton.y, &event.xbutton.state);
    }
    return event.xbutton.window;
}

Window findWindow(Display *d, Window current, char const *needle)
{
    Window ret = 0, root, parent, *children;
    unsigned cc;
    char *name = NULL;

    if(current == 0)
        current = XDefaultRootWindow(d);

    if(XFetchName(d, current, &name) > 0)
    {
        if(strstr(name, needle) != NULL)
        {
            XFree(name);
            return current;
        }
        XFree(name);
    }

    if(XQueryTree(d, current, &root, &parent, &children, &cc) != 0)
    {
        for(unsigned int i = 0; i < cc; ++i)
        {
            Window win = findWindow(d, children[i], needle);

            if(win != 0)
            {
                ret = win;
                break;
            }
        }
        XFree(children);
    }
    return ret;
}

void writePPM(const char* file, const unsigned char* data)
{
    FILE* f = fopen(file, "wb");
    if(f != NULL)
    {
        fprintf(f, "P6 28 28 255 ");
        fwrite(data, 1, slall, f);
        fclose(f);
    }
}

float sat(float x)
{
    if(x > 255.f)
        x = 255.f;
    if(x < 0.f)
        x = 0.f;
    return x;
}

void saveSample(Window w, const char* name)
{
    // get image block
    XImage* img = XGetImage(d, w, x-sw2, y-sh2, sw, sh, AllPlanes, XYPixmap);
    if(img == NULL)
        return;

    // colour map
    const Colormap map = XDefaultColormap(d, si);

    // extract colour information
    unsigned char rgbbytes[slall] = {0};
    int i = 0;
    for(int y = 0; y < sh; y++)
    {
        for(int x = 0; x < sw; x++)
        {
            XColor c;
            c.pixel = XGetPixel(img, x, y);
            XQueryColor(d, map, &c);

            // 0-255 scale
            rgbbytes[i]   = (unsigned char)sat((((((float)c.red)+1e-7f)   * 1.525900007e-05F) * 255.0f)+0.5f);
            rgbbytes[++i] = (unsigned char)sat((((((float)c.green)+1e-7f) * 1.525900007e-05F) * 255.0f)+0.5f);
            rgbbytes[++i] = (unsigned char)sat((((((float)c.blue)+1e-7f)  * 1.525900007e-05F) * 255.0f)+0.5f);
            i++;
        }
    }

    // free image block
    XFree(img);

    // save to file
    writePPM(name, &rgbbytes[0]);
}

XImage* img;
Colormap map;
const float tol = 64.f;
float ipit_score = 0.f;
static inline void ipit(const int x, const int y, const float a)
{
    XColor c;
    c.pixel = XGetPixel(img, x, y);
    XQueryColor(d, map, &c);
    const float r = sat((((((float)c.red)+1e-7f)   * 1.525900007e-05F) * 255.0f)+0.5f);
    const float g = sat((((((float)c.green)+1e-7f) * 1.525900007e-05F) * 255.0f)+0.5f);
    const float b = sat((((((float)c.blue)+1e-7f)  * 1.525900007e-05F) * 255.0f)+0.5f);
    const float gs = (r+g+b)/3.f;
    if(gs > a-tol && gs < a+tol)
        ipit_score += 1.f - (fabsf(gs-a) / tol);
}

static inline float fireScore()
{
    return ipit_score / 283.f;
}

void processScanArea(Window w)
{
    // get image block
    img = XGetImage(d, w, x-sw2, y-sh2, sw, sh, AllPlanes, XYPixmap);
    if(img == NULL)
        return;

    // colour map
    map = XDefaultColormap(d, si);

    // check pixels
    ipit_score = 0.f;
    ipit(0, 0, 77);
    ipit(2, 0, 76);
    ipit(3, 0, 76);
    ipit(7, 0, 76);
    ipit(8, 0, 75);
    ipit(9, 0, 75);
    ipit(17, 0, 75);
    ipit(0, 1, 77);
    ipit(1, 1, 76);
    ipit(2, 1, 76);
    ipit(3, 1, 76);
    ipit(6, 1, 75);
    ipit(7, 1, 75);
    ipit(8, 1, 75);
    ipit(12, 1, 75);
    ipit(13, 1, 74);
    ipit(14, 1, 74);
    ipit(18, 1, 74);
    ipit(19, 1, 74);
    ipit(25, 1, 76);
    ipit(0, 2, 77);
    ipit(1, 2, 76);
    ipit(2, 2, 76);
    ipit(7, 2, 74);
    ipit(10, 2, 73);
    ipit(12, 2, 73);
    ipit(13, 2, 73);
    ipit(14, 2, 73);
    ipit(15, 2, 73);
    ipit(18, 2, 72);
    ipit(19, 2, 72);
    ipit(27, 2, 77);
    ipit(1, 3, 75);
    ipit(2, 3, 75);
    ipit(6, 3, 73);
    ipit(7, 3, 73);
    ipit(8, 3, 73);
    ipit(10, 3, 71);
    ipit(11, 3, 71);
    ipit(12, 3, 72);
    ipit(13, 3, 71);
    ipit(14, 3, 70);
    ipit(15, 3, 70);
    ipit(16, 3, 70);
    ipit(17, 3, 70);
    ipit(18, 3, 70);
    ipit(19, 3, 71);
    ipit(20, 3, 72);
    ipit(3, 4, 74);
    ipit(6, 4, 72);
    ipit(7, 4, 72);
    ipit(8, 4, 71);
    ipit(11, 4, 70);
    ipit(12, 4, 69);
    ipit(14, 4, 68);
    ipit(15, 4, 68);
    ipit(16, 4, 68);
    ipit(19, 4, 70);
    ipit(20, 4, 71);
    ipit(21, 4, 72);
    ipit(25, 4, 75);
    ipit(27, 4, 76);
    ipit(6, 5, 71);
    ipit(12, 5, 67);
    ipit(13, 5, 67);
    ipit(14, 5, 66);
    ipit(18, 5, 67);
    ipit(19, 5, 68);
    ipit(20, 5, 69);
    ipit(22, 5, 72);
    ipit(6, 6, 69);
    ipit(7, 6, 69);
    ipit(9, 6, 66);
    ipit(10, 6, 66);
    ipit(12, 6, 64);
    ipit(13, 6, 63);
    ipit(15, 6, 64);
    ipit(22, 6, 71);
    ipit(23, 6, 72);
    ipit(24, 6, 73);
    ipit(0, 7, 75);
    ipit(5, 7, 69);
    ipit(7, 7, 68);
    ipit(8, 7, 67);
    ipit(9, 7, 65);
    ipit(11, 7, 63);
    ipit(12, 7, 62);
    ipit(13, 7, 61);
    ipit(18, 7, 61);
    ipit(18, 8, 58);
    ipit(19, 8, 60);
    ipit(1, 9, 74);
    ipit(6, 9, 66);
    ipit(10, 9, 58);
    ipit(11, 9, 55);
    ipit(26, 9, 73);
    ipit(1, 10, 73);
    ipit(5, 10, 67);
    ipit(9, 10, 58);
    ipit(10, 10, 56);
    ipit(11, 10, 53);
    ipit(12, 10, 51);
    ipit(13, 10, 49);
    ipit(19, 10, 57);
    ipit(5, 11, 66);
    ipit(10, 11, 54);
    ipit(12, 11, 48);
    ipit(19, 11, 55);
    ipit(5, 12, 65);
    ipit(16, 12, 46);
    ipit(18, 12, 51);
    ipit(4, 13, 65);
    ipit(5, 13, 63);
    ipit(8, 13, 56);
    ipit(18, 13, 50);
    ipit(19, 13, 54);
    ipit(20, 13, 55);
    ipit(0, 14, 72);
    ipit(3, 14, 67);
    ipit(11, 14, 47);
    ipit(13, 14, 43);
    ipit(17, 14, 46);
    ipit(19, 14, 52);
    ipit(20, 14, 54);
    ipit(21, 14, 58);
    ipit(22, 14, 61);
    ipit(3, 15, 66);
    ipit(7, 15, 56);
    ipit(9, 15, 50);
    ipit(11, 15, 46);
    ipit(12, 15, 44);
    ipit(24, 15, 65);
    ipit(3, 16, 65);
    ipit(4, 16, 63);
    ipit(13, 16, 42);
    ipit(14, 16, 42);
    ipit(15, 16, 42);
    ipit(16, 16, 44);
    ipit(17, 16, 45);
    ipit(19, 16, 51);
    ipit(20, 16, 54);
    ipit(3, 17, 64);
    ipit(4, 17, 61);
    ipit(9, 17, 48);
    ipit(12, 17, 42);
    ipit(13, 17, 41);
    ipit(14, 17, 41);
    ipit(15, 17, 42);
    ipit(22, 17, 58);
    ipit(23, 17, 60);
    ipit(25, 17, 64);
    ipit(1, 18, 67);
    ipit(3, 18, 63);
    ipit(6, 18, 56);
    ipit(11, 18, 43);
    ipit(12, 18, 41);
    ipit(14, 18, 42);
    ipit(15, 18, 42);
    ipit(16, 18, 43);
    ipit(17, 18, 45);
    ipit(18, 18, 47);
    ipit(20, 18, 52);
    ipit(21, 18, 55);
    ipit(24, 18, 61);
    ipit(0, 19, 66);
    ipit(12, 19, 42);
    ipit(16, 19, 44);
    ipit(21, 19, 54);
    ipit(25, 19, 62);
    ipit(9, 20, 47);
    ipit(10, 20, 44);
    ipit(11, 20, 42);
    ipit(13, 20, 42);
    ipit(17, 20, 44);
    ipit(20, 20, 50);
    ipit(24, 20, 59);
    ipit(25, 20, 61);
    ipit(26, 20, 62);
    ipit(27, 20, 64);
    ipit(5, 21, 55);
    ipit(10, 21, 44);
    ipit(12, 21, 42);
    ipit(13, 21, 41);
    ipit(14, 21, 41);
    ipit(15, 21, 42);
    ipit(16, 21, 42);
    ipit(17, 21, 43);
    ipit(18, 21, 45);
    ipit(19, 21, 47);
    ipit(20, 21, 49);
    ipit(23, 21, 56);
    ipit(25, 21, 59);
    ipit(5, 22, 56);
    ipit(6, 22, 53);
    ipit(7, 22, 51);
    ipit(13, 22, 41);
    ipit(14, 22, 41);
    ipit(15, 22, 41);
    ipit(16, 22, 42);
    ipit(17, 22, 43);
    ipit(18, 22, 44);
    ipit(19, 22, 46);
    ipit(20, 22, 48);
    ipit(24, 22, 57);
    ipit(25, 22, 59);
    ipit(26, 22, 61);
    ipit(27, 22, 63);
    ipit(8, 23, 48);
    ipit(9, 23, 45);
    ipit(12, 23, 41);
    ipit(13, 23, 41);
    ipit(14, 23, 41);
    ipit(15, 23, 41);
    ipit(16, 23, 42);
    ipit(17, 23, 44);
    ipit(18, 23, 45);
    ipit(19, 23, 46);
    ipit(20, 23, 48);
    ipit(21, 23, 49);
    ipit(25, 23, 58);
    ipit(26, 23, 60);
    ipit(27, 23, 62);
    ipit(7, 24, 50);
    ipit(9, 24, 44);
    ipit(12, 24, 41);
    ipit(13, 24, 40);
    ipit(14, 24, 40);
    ipit(15, 24, 41);
    ipit(16, 24, 42);
    ipit(17, 24, 43);
    ipit(18, 24, 44);
    ipit(19, 24, 45);
    ipit(20, 24, 47);
    ipit(21, 24, 49);
    ipit(22, 24, 51);
    ipit(24, 24, 55);
    ipit(25, 24, 57);
    ipit(26, 24, 59);
    ipit(4, 25, 56);
    ipit(7, 25, 49);
    ipit(11, 25, 41);
    ipit(12, 25, 41);
    ipit(13, 25, 40);
    ipit(14, 25, 40);
    ipit(15, 25, 41);
    ipit(17, 25, 42);
    ipit(18, 25, 44);
    ipit(19, 25, 45);
    ipit(20, 25, 46);
    ipit(21, 25, 48);
    ipit(22, 25, 50);
    ipit(23, 25, 52);
    ipit(24, 25, 54);
    ipit(27, 25, 60);
    ipit(0, 26, 62);
    ipit(5, 26, 53);
    ipit(6, 26, 50);
    ipit(7, 26, 48);
    ipit(8, 26, 46);
    ipit(9, 26, 44);
    ipit(10, 26, 42);
    ipit(11, 26, 42);
    ipit(12, 26, 41);
    ipit(13, 26, 40);
    ipit(14, 26, 40);
    ipit(15, 26, 41);
    ipit(16, 26, 42);
    ipit(17, 26, 42);
    ipit(18, 26, 43);
    ipit(20, 26, 46);
    ipit(21, 26, 48);
    ipit(5, 27, 51);
    ipit(6, 27, 49);
    ipit(7, 27, 47);
    ipit(8, 27, 46);
    ipit(9, 27, 44);
    ipit(11, 27, 42);
    ipit(14, 27, 41);
    ipit(15, 27, 41);
    ipit(16, 27, 42);
    ipit(17, 27, 42);
    ipit(18, 27, 43);
    ipit(24, 27, 53);

    // free image block
    XFree(img);
}

/***************************************************
   ~~ Console Utils
*/

void rainbow_printf(const char* text)
{
    static unsigned int base_clr = 0;
    if(base_clr == 0)
        base_clr = (rand()%125)+55;
    
    base_clr += 3;

    unsigned int clr = base_clr;
    const unsigned int len = strlen(text);
    for(unsigned int i = 0; i < len; i++)
    {
        clr++;
        printf("\e[38;5;%im", clr);
        printf("%c", text[i]);
    }
    printf("\e[38;5;123m");
}

void rainbow_line_printf(const char* text)
{
    static unsigned int base_clr = 0;
    if(base_clr == 0)
        base_clr = (rand()%125)+55;
    
    printf("\e[38;5;%im", base_clr);
    base_clr++;
    if(base_clr >= 230)
        base_clr = (rand()%125)+55;

    const unsigned int len = strlen(text);
    for(unsigned int i = 0; i < len; i++)
        printf("%c", text[i]);
    printf("\e[38;5;123m");
}

/***************************************************
   ~~ Program Entry Point
*/
int main()
{
    srand(time(0));
    signal(SIGPIPE, SIG_IGN);

    if(espeak_Initialize(AUDIO_OUTPUT_SYNCH_PLAYBACK, 0, 0, 0) < 0)
        espeak_fail = 1;

    printf("\e[1;1H\e[2J");
    rainbow_printf("James William Fletcher (james@voxdsp.com)\n\n");
    rainbow_printf("L-CTRL + L-ALT = Toggle BOT ON/OFF\n");
    rainbow_printf("R-CTRL + R-ALT = Toggle HOTKEYS ON/OFF\n\n");
    rainbow_printf("L-SHIFT + 3 = Toggle Missfire Reduction\n\n");
    rainbow_printf("P = Toggle crosshair\n\n");
    rainbow_printf("L = Toggle sample capture\n");
    rainbow_printf("Q = Capture non-target sample\n\n");
    rainbow_printf("G = Get activation for reticule area.\n");
    rainbow_printf("H = Get scans per second.\n");

    printf("\e[38;5;76m");
    printf("\nMake the crosshair a single green pixel or disable the game crosshair and use the crosshair provided by this bot .. or if your monitor provides a crosshair use that.\n\n");
    printf("This bot will only auto trigger when W,A,S,D are not being pressed. (so when your not moving in game, aka stationary)\n\n");
    printf("\e[38;5;123m");
    
    xdo_t* xdo;
    XColor c[9];
    GC gc = 0;
    unsigned int enable = 0;
    unsigned int offset = 3;
    unsigned int crosshair = 0;
    unsigned int sample_capture = 0;
    unsigned int hotkeys = 1;
    unsigned int draw_sa = 0;
    time_t ct = time(0);

    // open display 0
    d = XOpenDisplay(":0");
    if(d == NULL)
    {
        printf("Failed to open display\n");
        return 0;
    }

    // get default screen
    si = XDefaultScreen(d);

    // find window
    twin = findWindow(d, 0, "Counter-Strike");
    if(twin != 0)
        printf("CS:GO Win: 0x%lX\n", twin);

    //xdo
    xdo = xdo_new(":0.0");

    // set console title
    Window awin;
    xdo_get_active_window(xdo, &awin);
    xdo_set_window_property(xdo, awin, "WM_NAME", "CS:GO PLATINUM");

    // get graphics context
    gc = DefaultGC(d, si);
    
    while(1)
    {
        // loop every 1 ms (1,000 microsecond = 1 millisecond)
        usleep(SCAN_DELAY);

        // inputs
        if(key_is_pressed(d, XK_Control_L) && key_is_pressed(d, XK_Alt_L))
        {
            if(enable == 0)
            {                
                // get window
                twin = findWindow(d, 0, "Counter-Strike");

                // get center window point (x & y)
                XWindowAttributes attr;
                XGetWindowAttributes(d, twin, &attr);
                x = attr.width/2;
                y = attr.height/2;

                // toggle
                enable = 1;
                usleep(300000);
                rainbow_line_printf("BOT: ON ");
                printf("[%ix%i]\n", x, y);
                speakS("on.");
            }
            else
            {
                enable = 0;
                usleep(300000);
                rainbow_line_printf("BOT: OFF\n");
                speakS("off.");
            }
        }
        
        // bot on/off
        if(enable == 1)
        {
            // input toggle
            if(key_is_pressed(d, XK_Control_R) && key_is_pressed(d, XK_Alt_R))
            {
                if(hotkeys == 0)
                {
                    hotkeys = 1;
                    usleep(300000);
                    printf("HOTKEYS: ON [%ix%i]\n", x, y);
                    speakS("hk on.");
                }
                else
                {
                    hotkeys = 0;
                    usleep(300000);
                    rainbow_line_printf("HOTKEYS: OFF\n");
                    speakS("hk off.");
                }
            }

            if(hotkeys == 1)
            {
                // crosshair toggle
                if(key_is_pressed(d, XK_P))
                {
                    if(crosshair == 0)
                    {
                        crosshair = 1;
                        usleep(300000);
                        rainbow_line_printf("CROSSHAIR: ON\n");
                        speakS("cx on.");
                    }
                    else
                    {
                        crosshair = 0;
                        usleep(300000);
                        rainbow_line_printf("CROSSHAIR: OFF\n");
                        speakS("cx off.");
                    }
                }

                static uint64_t scd = 0;
                if(key_is_pressed(d, XK_Q) && sample_capture == 1 && microtime() > scd)
                {
                    char name[32];
                    sprintf(name, "%s/%i.ppm", nontargets_dir, rand());
                    saveSample(twin, name);

                    draw_sa = 100;

                    scd = microtime() + 350000;
                }

                if(key_is_pressed(d, XK_Shift_L) && key_is_pressed(d, XK_3))
                {
                    if(REPEAT_ACTIVATION == 6)
                    {
                        REPEAT_ACTIVATION = 0;
                        rainbow_line_printf("Miss Fire Reduction Off.\n");
                        speakS("Miss Fire Reduction Off.");
                    }
                    else if(REPEAT_ACTIVATION == 3)
                    {
                        REPEAT_ACTIVATION = 6;
                        rainbow_line_printf("Miss Fire Reduction High.\n");
                        speakS("Miss Fire Reduction High.");
                    }
                    else
                    {
                        REPEAT_ACTIVATION = 3;
                        rainbow_line_printf("Miss Fire Reduction Medium.\n");
                        speakS("Miss Fire Reduction Medium.");
                    }
                }

                // sample capture toggle
                if(key_is_pressed(d, XK_L))
                {
                    if(sample_capture == 0)
                    {
                        char* home = getenv("HOME");
                        sprintf(targets_dir, "%s/Desktop/targets", home);
                        mkdir(targets_dir, 0777);
                        sprintf(nontargets_dir, "%s/Desktop/nontargets", home);
                        mkdir(nontargets_dir, 0777);

                        sample_capture = 1;
                        usleep(300000);
                        rainbow_line_printf("SAMPLE CAPTURE: ON\n");
                        speakS("sc on.");
                    }
                    else
                    {
                        sample_capture = 0;
                        usleep(300000);
                        rainbow_line_printf("SAMPLE CAPTURE: OFF\n");
                        speakS("sc off.");
                    }
                }

                if(key_is_pressed(d, XK_G)) // print activation when pressed
                {
                    processScanArea(twin);
                    const float ret = fireScore();
                    if(ret >= ACTIVATION_SENITIVITY)
                    {
                        printf("\e[93mA: %f\e[0m\n", ret);
                        XSetForeground(d, gc, 65280);
                        XDrawRectangle(d, twin, gc, x-sw2-1, y-sh2-1, sw+2, sh+2);
                        XSetForeground(d, gc, 0);
                        XDrawRectangle(d, twin, gc, x-sw2-2, y-sh2-2, sw+4, sh+4);
                        XFlush(d);
                    }
                    else
                    {
                        printf("A: %f\n", ret);
                        XSetForeground(d, gc, 16711680);
                        XDrawRectangle(d, twin, gc, x-sw2-1, y-sh2-1, sw+2, sh+2);
                        XSetForeground(d, gc, 0);
                        XDrawRectangle(d, twin, gc, x-sw2-2, y-sh2-2, sw+4, sh+4);
                        XFlush(d);
                    }
                }

                if(key_is_pressed(d, XK_H)) // print samples per second when pressed
                {
                    static uint64_t st = 0;
                    static uint sc = 0;
                    processScanArea(twin);
                    const float ret = fireScore();
                    sc++;
                    if(microtime() - st >= 1000000)
                    {
                        printf("\e[36mSPS: %u\e[0m\n", sc);
                        sc = 0;
                        st = microtime();
                    }              
                }
            }

            static uint64_t rft = 0;
            if(key_is_pressed(d, XK_W) == 0 && key_is_pressed(d, XK_A) == 0 && key_is_pressed(d, XK_S) == 0 && key_is_pressed(d, XK_D) == 0 && microtime() > rft)
            {
                processScanArea(twin);
                const float activation = fireScore();
                if(activation >= ACTIVATION_SENITIVITY)
                {
                    tc++;

                    // did we activate enough times in a row to be sure this is a target?
                    if(tc > REPEAT_ACTIVATION)
                    {
                        if(sample_capture == 1)
                        {
                            char name[32];
                            sprintf(name, "%s/%i.ppm", targets_dir, rand());
                            saveSample(twin, name);
                        }

                        xdo_mouse_down(xdo, CURRENTWINDOW, 1);
                        usleep(100000);
                        xdo_mouse_up(xdo, CURRENTWINDOW, 1);

                        if(sample_capture == 1)
                            rft = microtime() + 350000;
                        else
                            rft = microtime() + 80000;

                        // display ~1s recharge time
                        if(crosshair != 0)
                        {
                            crosshair = 2;
                            ct = time(0);
                        }
                    }
                }
                else
                {
                    tc = 0;
                }
            }

            if(crosshair == 1)
            {
                if(sample_capture == 1)
                {
                    if(draw_sa > 0)
                    {
                        // draw sample outline
                        XSetForeground(d, gc, 16711680);
                        XDrawRectangle(d, twin, gc, x-sw2-1, y-sh2-1, sw+2, sh+2);
                        XSetForeground(d, gc, 16711680);
                        XDrawRectangle(d, twin, gc, x-sw2-2, y-sh2-2, sw+4, sh+4);
                        XFlush(d);
                        draw_sa -= 1;
                    }
                    else
                    {
                        XSetForeground(d, gc, 16777215);
                        XDrawRectangle(d, twin, gc, x-sw2-1, y-sh2-1, sw+2, sh+2);
                        XSetForeground(d, gc, 16776960);
                        XDrawRectangle(d, twin, gc, x-sw2-2, y-sh2-2, sw+4, sh+4);
                        XFlush(d);
                    }
                }
                else
                {
                    XSetForeground(d, gc, 65280);
                    XDrawRectangle(d, twin, gc, x-sw2-1, y-sh2-1, sw+2, sh+2);
                    XFlush(d);
                }
            }

            if(crosshair == 2)
            {
                if(time(0) > ct+1)
                    crosshair = 1;

                if(sample_capture == 1)
                {
                    XSetForeground(d, gc, 16777215);
                    XDrawRectangle(d, twin, gc, x-sw2-1, y-sh2-1, sw+2, sh+2);
                    XSetForeground(d, gc, 16711680);
                    XDrawRectangle(d, twin, gc, x-sw2-2, y-sh2-2, sw+4, sh+4);
                    XFlush(d);
                }
                else
                {
                    XSetForeground(d, gc, 16711680);
                    XDrawRectangle(d, twin, gc, x-sw2-1, y-sh2-1, sw+2, sh+2);
                    XSetForeground(d, gc, 65280);
                    XDrawRectangle(d, twin, gc, x-sw2-2, y-sh2-2, sw+4, sh+4);
                    XFlush(d);
                }
            }
        }

        //
    }

    // done, never gets here in regular execution flow
    XCloseDisplay(d);
    return 0;
}
