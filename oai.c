/*
 * View images inside your terminal window
 * -olealgoritme, 2020/2021
 */

#include <err.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <xcb/xcb.h>
#include <xcb/xproto.h>
#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <cairo/cairo.h>
#include <cairo/cairo-xcb.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define MOVEMENT_PIXEL_STEP 20 /* 20 pixel movement */

#define DEBUG 0

#if defined(DEBUG) && DEBUG > 0
 #define debug_print(fmt, args...) fprintf(stderr, "DEBUG: %s:%d:%s(): " fmt, \
    __FILE__, __LINE__, __func__, ##args)
#else
 #define debug_print(fmt, args...)
#endif


enum {
    NET_WM_WINDOW_TYPE,
    NET_WM_WINDOW_TYPE_DOCK,
    NET_WM_STATE,
    NET_WM_STATE_ABOVE,
    NET_WM_DESKTOP,
};


struct win
{
    uint16_t width;
    uint16_t height;
    uint16_t x;
    uint16_t y;
};


struct cairo_image
{
    cairo_surface_t *surface;
    int            format;
    void           *data;
    int            width;
    int            height;
    int            n_components;
    int            stride;
};


struct xcb_display
{
    xcb_connection_t *c;
    xcb_screen_t     *s;
    xcb_visualtype_t *v;
};

Display * display;
struct win root_win;

struct xcb_display display_info;
xcb_window_t parent_window;
xcb_window_t window = {0};
cairo_t *cr = NULL;
cairo_surface_t *window_surface = NULL;
struct cairo_image image;

double fill_percent = 0.9;
double scale = 1.0;
int image_height = 0;
int image_width = 0;
int x = 0;
int y = 0;

static xcb_visualtype_t * get_alpha_visualtype(xcb_screen_t *s)
{
    // return first visual type with 32bit depth
    xcb_depth_iterator_t di = xcb_screen_allowed_depths_iterator(s);

    for( ; di.rem; xcb_depth_next (&di) ) {
        if( di.data->depth == 32 ) {
            return xcb_depth_visuals_iterator(di.data).data;
        }
    }
    return NULL;
}


static void xcb_init(struct xcb_display *display_info)
{

    display_info->c = xcb_connect(NULL, NULL);
    if(xcb_connection_has_error(display_info->c))
        errx(1, "couldn't connect to X");

    display_info->s = xcb_setup_roots_iterator(xcb_get_setup(display_info->c)).data;
    if(display_info->s == NULL)
        errx(1, "couldn't find the screen");

    display_info->v = get_alpha_visualtype(display_info->s);
    if(display_info->v == NULL)
        errx(1, "transparency support not found");

}


static struct win xcb_get_window_geometry(xcb_connection_t *c, xcb_window_t window)
{
    struct win win = {0};
    xcb_get_geometry_cookie_t cookie;
    xcb_get_geometry_reply_t *reply;
    cookie = xcb_get_geometry(c, window);
    if ((reply = xcb_get_geometry_reply(c, cookie, NULL))) {
        debug_print("Root window relative position is at %d,%d\n", reply->x, reply->y);
        debug_print("Root window size is %dx%d\n", reply->width, reply->height);
        win.x = reply->x;
        win.y = reply->y;
        win.width = reply->width;
        win.height = reply->height;
    }
    free(reply);
    return win;
}


static void xcb_set_opacity(double opacity, xcb_connection_t *c, xcb_window_t win)
{
    xcb_intern_atom_cookie_t opacity_cookie = xcb_intern_atom(c, 0, strlen ( "_NET_WM_WINDOW_OPACITY" ), "_NET_WM_WINDOW_OPACITY" );
    xcb_intern_atom_reply_t *opacity_reply = xcb_intern_atom_reply ( c, opacity_cookie, NULL );
    uint32_t real_opacity = opacity * 0xffffffff;

    xcb_change_property(c ,
                        XCB_PROP_MODE_REPLACE,
                        win,
                        opacity_reply->atom,
                        XCB_ATOM_CARDINAL,
                        32,
                        1L,
                        &real_opacity);
}


static void xcb_remove_decorations(xcb_connection_t *c, xcb_window_t win)
{
    xcb_intern_atom_cookie_t cookie3 = xcb_intern_atom(c, 0, strlen ( "_MOTIF_WM_HINTS" ), "_MOTIF_WM_HINTS" );
    xcb_intern_atom_reply_t *reply3 = xcb_intern_atom_reply ( c, cookie3, NULL );

    // motif hints
    struct MotifHints
        {
        uint32_t   flags;
        uint32_t   functions;
        uint32_t   decorations;
        int32_t    input_mode;
        uint32_t   status;
        };

    struct MotifHints hints;

    hints.flags = 2;
    hints.functions = 0;
    hints.decorations = 0;
    hints.input_mode = 0;
    hints.status = 0;

    xcb_change_property ( c,
                         XCB_PROP_MODE_REPLACE,
                         win,
                         reply3->atom,
                         reply3->atom,
                         32,
                         5,
                         &hints );
    free(reply3);
}


static Window xcb_get_root_window(Display *d)
{
    Window w;
    int r;
    XGetInputFocus(d, &w, &r);
    if (w != None) {
        debug_print("Root Window id: 0x%x8\n", (int) w);
        return w;
    }
    return w;
}


static xcb_window_t create_window(long parent_window_id, struct xcb_display display_info, int x, int y, int width, int height)
{
    // generate colormap for window with alpha support
    xcb_window_t colormap = xcb_generate_id(display_info.c);
    xcb_create_colormap(display_info.c, XCB_COLORMAP_ALLOC_NONE, colormap, display_info.s->root, display_info.v->visual_id);
    const int dock = 1,
              depth = 32,
              border_width = 10;

    // generate id
    xcb_window_t window = xcb_generate_id(display_info.c);
    const uint32_t vals[] = {
        0x553D3DFF,
        0x553D3D77,
        dock,
        XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_BUTTON_PRESS | XCB_EVENT_MASK_PROPERTY_CHANGE | XCB_EVENT_MASK_KEY_PRESS,
        colormap
    };

    // create window
    xcb_create_window(display_info.c,
                      depth,                                // depth
                      window,                               // xcb_window_t type
                      parent_window_id,                     // parent window (id)
                      x,                                    // window X coord
                      y,                                    // window Y coord
                      width,                                // window width
                      height,                               // window height
                      border_width,                         // border_width
                      XCB_WINDOW_CLASS_INPUT_OUTPUT,
                      display_info.v->visual_id,
                      XCB_CW_BACK_PIXEL | XCB_CW_BORDER_PIXEL | XCB_CW_OVERRIDE_REDIRECT | XCB_CW_EVENT_MASK | XCB_CW_COLORMAP,
                      vals);

    xcb_remove_decorations(display_info.c, window);
    //xcb_set_opacity(0.50, display_info.c, window);
    xcb_free_colormap(display_info.c, colormap);

    return window;
}


static void show_window (xcb_connection_t *c, xcb_window_t window, int x, int y)
{
    // atom names that we want to find atoms for
    const char *atom_names[] = {
        "_NET_WM_WINDOW_TYPE",
        "_NET_WM_WINDOW_TYPE_DOCK",
        "_NET_WM_STATE",
        "_NET_WM_STATE_ABOVE",
        "_NET_WM_DESKTOP",
        "_NET_WM_WINDOW_TYPE_DOCK",
    };

    // get all the atoms
    const int atoms = sizeof(atom_names)/sizeof(char *);
    xcb_intern_atom_cookie_t atom_cookies[atoms];
    xcb_atom_t atom_list[atoms];
    xcb_intern_atom_reply_t *atom_reply;

    int i;
    for (i = 0; i < atoms; i++)
        atom_cookies[i] = xcb_intern_atom(c, 0, strlen(atom_names[i]), atom_names[i]);

    for (i = 0; i < atoms; i++) {
        atom_reply = xcb_intern_atom_reply(c, atom_cookies[i], NULL);
        if (!atom_reply)
            printf("failed to find atom: %s\n", atom_names[i]);
        atom_list[i] = atom_reply->atom;
        free(atom_reply);
    }

    // set the atoms
    const uint32_t desktops[] = {-1};
    xcb_change_property(c, XCB_PROP_MODE_REPLACE, window, atom_list[NET_WM_WINDOW_TYPE],
                        XCB_ATOM_ATOM, 32, 1, &atom_list[NET_WM_WINDOW_TYPE_DOCK]);
    xcb_change_property(c, XCB_PROP_MODE_APPEND, window, atom_list[NET_WM_STATE],
                        XCB_ATOM_ATOM, 32, 2, &atom_list[NET_WM_STATE_ABOVE]);
    xcb_change_property(c, XCB_PROP_MODE_REPLACE, window, atom_list[NET_WM_DESKTOP],
                        XCB_ATOM_CARDINAL, 32, 1, desktops);

    const uint32_t val[] = { 1 };
    xcb_change_window_attributes(c, window, XCB_CW_OVERRIDE_REDIRECT, val);

    // map xcb window
    xcb_map_window(c, window);
    const uint32_t vals[] = {x,y};
    xcb_configure_window(c, window, XCB_CONFIG_WINDOW_X | XCB_CONFIG_WINDOW_Y, vals);
    xcb_flush(c);
}


xcb_atom_t intern_atom(xcb_connection_t *conn, const char *atom)
{
    xcb_atom_t result = XCB_NONE;
    xcb_intern_atom_reply_t *r = xcb_intern_atom_reply(conn,
                                                       xcb_intern_atom(conn, 0, strlen(atom), atom),
                                                       NULL);
    if (r)
        result = r->atom;
    free(r);
    return result;
}

void cairo_draw (cairo_t *cr, cairo_surface_t *surface)
{
    cairo_set_source_surface(cr, surface, 0, 0);
    cairo_paint(cr);
}

void cairo_read_image(char * filename, struct cairo_image *image)
{

    int len = strlen((const char *) filename);
    image->format = CAIRO_FORMAT_ARGB32;

    if(len >= 4 && !strcasecmp(&filename[len- 4], ".png")) {
        image->surface = cairo_image_surface_create_from_png(filename);
    } else {
        int depth = 4; //  4 bytes (8bits x 4 = 32bpp)
        image->data = stbi_load(filename, &image->width, &image->height, &image->n_components, depth);
        if ( image->data == NULL )
            errx(1, "ERROR opening file: %s\n", filename);
        image->format = CAIRO_FORMAT_ARGB32;
        image->stride = cairo_format_stride_for_width(image->format, image->width);
        image->surface = cairo_image_surface_create_for_data(image->data,
                                                             image->format,
                                                             image->width,
                                                             image->height,
                                                             image->stride);
    }
}

int suffix_check(char *filename)
{
    const char *suffix_list[] = { "jpg", "jpeg", "png", "bmp", "gif" };
    char *suffix = strrchr(filename, '.');
    if (suffix) {
        suffix++;
        for(size_t i = 0; i < sizeof(suffix_list); i++) {
            if (!strcasecmp(suffix, suffix_list[i])) {
                debug_print("got ext: %s\n", suffix_list[i]);
                return 1;
            }
        }
        return 0;
    }
    return -1;
}


void repaint_window(cairo_surface_t *surface, cairo_surface_t *window_surface)
{
    cairo_surface_flush(surface);
    cairo_t *cr = cairo_create(window_surface);
    cairo_set_source_surface(cr, window_surface, 0, 0);
    cairo_paint(cr);
    cairo_destroy(cr);
    cairo_surface_flush(window_surface);
 }


// simple mpv embed into window (not implemented)
void mpv_play (const char *filename, Window window, FILE **mpv)
{
  char cmd[1024];
  snprintf(cmd, 1024, "mpv --wid 0x%lx %s", window, filename);
  *mpv = popen(cmd, "w");
}

void render_window(int x, int y, int image_width, int image_height)
{
    // create xcb and cairo surfaces
    xcb_destroy_window(display_info.c, window);
    window = create_window(parent_window, display_info, x, y, image_width, image_height);
    window_surface = cairo_xcb_surface_create(display_info.c, window, display_info.v, image_width, image_height);
    cr = cairo_create(window_surface);
    cairo_set_source_surface(cr, window_surface, 0, 0);
    cairo_paint(cr);

    cairo_scale(cr, scale, scale);
    //cairo_surface_flush(window_surface);

    // configure xcb window and map it
    show_window(display_info.c, window, x, y);
}

void print_usage()
{
    printf("Usage: oai [-h, --help] [-s, --scale] [-x position] [-y position] imagefile\n");
      puts("--------------------------------------------------------------------------------");
      puts("    -h, --help       print this message");
      puts("    -s               set image scaling factor");
      puts("    -x               set image at x position");
      puts("    -y               set image at y position");
}


int main (int argc, char **argv)
{
    char *filename;
    int help_flag = 0;
    int error_flag = 0;
    int option = 0;
    int option_index = 0;

    struct option long_options[] =
    {
            {"help",     no_argument,       &help_flag,       1},
            {"scale",    required_argument, 0,              's'},
            {"debug",    no_argument,       0,                0},
            {0, 0, 0, 0}
    };

    while (option != -1) {
        option = getopt_long(argc, argv, "h:d:s:x:y:", long_options, &option_index);

        switch ( option ) {
            case 'h':
                option = -1;
                help_flag = 1;
                break;

            case 's':
                scale = atof(optarg);
                break;

            case 'x':
                x = atoi(optarg);
                break;

            case 'y':
                y = atoi(optarg);
                break;

            case '?':
                error_flag = 1;
                break;
        }
    }

    if (help_flag) {
        print_usage();
        return 0;
    }

    if (error_flag)
        return 1;

    if (!(optind < argc)) {
        errx(1, "ERR: No image file specified");
    }

    if ((optind+1) != argc) {
        errx(1, "ERR: Unexpected argument");
    }

    filename = argv[optind];
    if (suffix_check(filename)< 0) {
        errx(1, "ERR: Wrong image file type");
    }

    // load timer
    clock_t start = clock();

    // read image
    cairo_read_image(filename, &image);

    // initialize xcb parameters
    xcb_init(&display_info);

    // open display
    display = XOpenDisplay(NULL);
    if ( !display )
        errx(1, "ERR: Cant' open display");

    // find parent window
    parent_window = xcb_get_root_window(display);
    if (!parent_window)
        errx(1, "ERR: Cant' attach to parent window");

    root_win = xcb_get_window_geometry(display_info.c, parent_window);

    // get image width and height
    image_width  = (cairo_image_surface_get_width(image.surface) * scale);
    image_height = (cairo_image_surface_get_height(image.surface) * scale);

    int max_width = root_win.width * fill_percent;
    int max_height = root_win.height * fill_percent;

    if (image_width >= max_width)
        image_width = max_width;

    if (image_height >= max_height)
        image_height = max_height;

    // calculate center position
    x = (root_win.width / 2) - (image_width / 2);
    y = (root_win.height / 2) - (image_height / 2);

    render_window(x, y, image_width, image_height);

    if ( DEBUG > 0 ) {
        debug_print("oai window id: 0x%08x\n", window);
        fflush(stdout);
    }

    // xcb generic event struct
    xcb_generic_event_t *ev;

    while ( (ev = xcb_wait_for_event(display_info.c)) ) {

        switch(ev->response_type & ~0x80) {

            case XCB_EXPOSE:
            {
                cairo_draw(cr, image.surface);
                cairo_surface_flush(image.surface);

                xcb_set_input_focus(display_info.c, XCB_INPUT_FOCUS_PARENT, window, 0);
                xcb_flush(display_info.c);

                clock_t diff = clock() - start;
                int msec = diff * 1000 / CLOCKS_PER_SEC;
                debug_print("Time taken %d seconds %d milliseconds\n", msec/1000, msec%1000);
                break;
            }

            // mouse click = QUIT
            case XCB_BUTTON_PRESS:
            {
                xcb_button_press_event_t *buttonEvt = (xcb_button_press_event_t *) ev;
                xcb_button_t button = buttonEvt->detail;

                // quit on click
                if (button == XCB_BUTTON_INDEX_1 || button == XCB_BUTTON_INDEX_2 || button == XCB_BUTTON_INDEX_3) {
                    debug_print("QUIT\n");
                    xcb_set_input_focus(display_info.c, XCB_INPUT_FOCUS_PARENT, parent_window, 0);
                    goto END;
                }
                break;
            }

            case XCB_KEY_PRESS:
            {
                xcb_key_press_event_t *keyEv = (xcb_key_press_event_t *) ev;

                // translate keycode -> keysymbol -> string
                xcb_keycode_t code = keyEv->detail;
                KeySym keysym = XkbKeycodeToKeysym(display, code, 0, 0);
                const char *ch = XKeysymToString(keysym);

                // plus
                if(keysym == 0x002b) {
                    debug_print("pressed +\n");
                    scale *= 1.1;
                    image_width *= 1.1;
                    image_height *= 1.1;
                    render_window(x, y, image_width, image_height);
                    break;
                }
                else 
                // minus
                if (keysym == 0x002d) {
                    debug_print("pressed -\n");
                    scale *= 0.9;
                    image_width *= 0.9;
                    image_height *= 0.9;
                    render_window(x, y, image_width, image_height);
                    break;
                }
                else 
                // up arrow
                if (keysym == 0xff52 || keysym == 0x08fc) {
                    debug_print("pressed up\n");
                    y -= MOVEMENT_PIXEL_STEP;
                    render_window(x, y, image_width, image_height);
                    break;
                }
                else 
                // right arrow
                if (keysym == 0xff53 || keysym == 0x09f5) {
                    debug_print("pressed right\n");
                    x += MOVEMENT_PIXEL_STEP;
                    render_window(x, y, image_width, image_height);
                    break;
                }
                else 
                // down arrow
                if (keysym == 0xff54 || keysym == 0xff98) {
                    debug_print("pressed down\n");
                    y += MOVEMENT_PIXEL_STEP;
                    render_window(x, y, image_width, image_height);
                    break;
                }
                else 
                // left arrow
                if (keysym == 0xff51 || keysym == 0xff96) {
                    debug_print("pressed left\n");
                    x -= MOVEMENT_PIXEL_STEP;
                    render_window(x, y, image_width, image_height);
                    break;
                }
                else 
                // "q" to quit
                if (keysym == 0x0071) {
                    debug_print("key: %s\n", ch);
                    debug_print("QUIT\n");
                    goto END;
                }
                xcb_flush(display_info.c);
                break;
            }
        }
    }

END:
{
    cairo_surface_destroy(image.surface);
    cairo_surface_destroy(window_surface);
    cairo_destroy(cr);
    xcb_destroy_window(display_info.c, window);
    xcb_disconnect(display_info.c);
    XCloseDisplay(display);
    display_info.c = NULL;
}

    return 0;
}
