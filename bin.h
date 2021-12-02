#include "alg/alg.h"
#include "acg/trs.h"
#include "acg/ds.h"

#include "GLFW/glfw3.h"
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include "cimgui/cimgui.h"

#define FIXED_DT (1.f / 120.f)
#define ACG_REAL double
#include "acg/time.h"

extern struct render {
	v3 col;
	int wire;
	abuf *meshes;
	abuf *lines;
} render;

extern struct frame {
	float w, h;
} frame;

#include "acg/cam.h"
extern cam3 cam;

typedef struct {
	enum {
		  CURSOR_LOCKED
		, CURSOR_INF
	} cursor;
	enum {
		  RUBBISH_CFG_NONE = 0x0
		, RUBBISH_CFG_WIRE = 0x1
		, RUBBISH_CFG_AA   = 0x2
		, RUBBISH_CFG_LINE_PERSIST = 0x4
		, RUBBISH_CFG_UI_MOUSEONLY = 0x8
	} flags;
	u8 crush;
	struct shader {
		unsigned int n;
		char *raw;
	} vert, frag;
	float line_width;
} rubbish_cfg;

void rubbish_run(
	const rubbish_cfg,
	void (*init)(void),
	void (*update)(void),
	void (*update_fixed)(void)
);

typedef struct {
	u32 n; // Tris
	u32 size;
	ttt trs;
	fff *pts;
	fff vfx;
	int wire;

	union {
		fff col;
		fff cols[4];
	};
} mesh;

typedef struct {
	v3 beg;
	v3 end;
	v3 col;
} line;

mesh mesh_new(const u32 n, void*);
void mesh_free(mesh*);
void meshes_push(const mesh*);
void lines_push(const fff, const fff, const fff);

extern struct mouse {
	ff pos;
	ff delta;
	float wheel;
} mouse;

int key_held(const int);
int key_press(const int);
int key_release(const int);

int btn_held(const int);
int btn_press(const int);
int btn_release(const int);

#define KEY(E)    (key_held   (GLFW_KEY_ ## E))
#define KEY_UP(E) (key_release(GLFW_KEY_ ## E))
#define KEY_DN(E) (key_press  (GLFW_KEY_ ## E))

#define BTN(E)    (btn_held   (GLFW_MOUSE_BUTTON_ ## E))
#define BTN_UP(E) (btn_release(GLFW_MOUSE_BUTTON_ ## E))
#define BTN_DN(E) (btn_press  (GLFW_MOUSE_BUTTON_ ## E))

void set_cursor(const int);
