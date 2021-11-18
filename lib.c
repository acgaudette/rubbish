#ifdef DEBUG_FPE
#ifdef __linux__
	#define _GNU_SOURCE
#else
	#undef DEBUG_FPE
#endif
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#ifdef DEBUG_FPE
#include <signal.h>
#include <fenv.h>
static void fpehandler(int _)
{
	fprintf(stderr, "caught FP exception\n");
	fflush(stdout);
	abort();
}
#endif

#define GLFW_INCLUDE_NONE
#include "GL/gl3w.h"
#include "GLFW/glfw3.h"
static GLFWwindow *win;

#define SIZE_LINE (2 * sizeof(fff))
#include "bin.h"
struct _time _time = { .scale = 1.f };

#include "acg/sys.h"

#ifdef RUBBISH_IMGUI
	#include "cimgui/cimgui_impl.h"
	static ImGuiContext *imgui;
	static ImGuiIO *imgui_io;
#endif

static u32 keys[GLFW_KEY_LAST];
static u32 buttons[GLFW_MOUSE_BUTTON_LAST];

int key_held(const int e)
{
	return glfwGetKey(win, e) == GLFW_PRESS;
}

int btn_held(const int e)
{
	return glfwGetMouseButton(win, e) == GLFW_PRESS;
}

static void key_inc(const int e)
{
	const int held = key_held(e);
	keys[e] <<= 16;
	keys[e] |= held * 0xffff;
}

static void btn_inc(const int e)
{
	const int held = btn_held(e);
	buttons[e] <<= 16;
	buttons[e] |= held * 0xffff;
}

int key_press(const int e)
{
	return 0xffff == keys[e];
}

int key_release(const int e)
{
	return 0xffff0000 == keys[e];
}

int btn_press(const int e)
{
	return 0xffff == buttons[e];
}

int btn_release(const int e)
{
	return 0xffff0000 == buttons[e];
}

struct render render;
struct frame frame;
struct mouse mouse;
cam3 cam = CAM3_DEFAULT;

struct {
	GLint vp;
	GLint model;
	GLint col;
	GLint clear;
} unif;

void lines_push(const fff a, const fff b, const fff col)
{
	*(line*)abuf_push(render.lines) = (line) { a, b, col };
}

mesh mesh_new(const u32 n)
{
	u32 size = n * 3 * sizeof(fff);
	fff *pts = malloc(size);
	assert(pts);

	return (mesh) {
		.n = n,
		.size = size,
		.trs = T3_ID,
		.pts = pts,
		.col = V3_ONE,
		.wire = 0,
	};
}

void meshes_push(const mesh *in)
{
	if (!in->pts)
		return;
	*(mesh*)abuf_push(render.meshes) = *in;
}

static void frame_clear()
{
	abuf_clear(render.meshes);
	abuf_clear(render.lines);
}

static void log_gl(
	GLenum  _0,
	GLenum  _1,
	GLuint  _2,
	GLenum level,
	GLsizei _4,
	const GLchar *msg,
	const void* _5)
{
	switch (level) {
	case GL_DEBUG_SEVERITY_NOTIFICATION:
		return;
	default:
		break;
	}

	fprintf(stderr, "gl: %s\n", msg);
}

#include "vert.h"
#include "frag.h"
GLuint shader_init(GLenum type)
{
	const char *dbg;
	const GLchar *buf;
	GLint len;

	switch (type) {
	case GL_VERTEX_SHADER:
		buf = (char*)vert_glsl;
		len = vert_glsl_len;
		dbg = "vert";
		break;
	case GL_FRAGMENT_SHADER:
		buf = (char*)frag_glsl;
		len = frag_glsl_len;
		dbg = "frag";
		break;
	default:
		panic();
	}

	GLuint sh = glCreateShader(type);
	glShaderSource(sh, 1, &buf, &len);
	glCompileShader(sh);

	static char log[128];
	memset(log, 0, 128);
	int result;

	glGetShaderiv(sh, GL_COMPILE_STATUS, &result);
	if (!result) {
		glGetShaderInfoLog(sh, 128, NULL, log);
		fprintf(stderr, "[compile] %s shader: \"%s\"\n", dbg, log);
		panic();
	}

	return sh;
}

static void shaders_init()
{
	const GLuint vert = shader_init(GL_VERTEX_SHADER);
	const GLuint frag = shader_init(GL_FRAGMENT_SHADER);

	GLuint prog = glCreateProgram();
	glAttachShader(prog, vert);
	glAttachShader(prog, frag);
	glLinkProgram(prog);

	static char log[128];
	memset(log, 0, 128);
	int result;

	glGetProgramiv(prog, GL_LINK_STATUS, &result);
	if (!result) {
		glGetProgramInfoLog(prog, 128, NULL, log);
		fprintf(stderr, "[link] shaders: \"%s\"\n", log);
		panic();
	}

	unif.vp    = glGetUniformLocation(prog, "vp");
	unif.model = glGetUniformLocation(prog, "trs");
	unif.col   = glGetUniformLocation(prog, "col");
	unif.clear = glGetUniformLocation(prog, "col_clear");

	glUseProgram(prog);
	glDeleteShader(vert);
	glDeleteShader(frag);
}

static float mouse_dirty;
void set_cursor(const int e)
{
	int cursor;
	switch (e) {
	case CURSOR_LOCKED:
		cursor = GLFW_CURSOR_HIDDEN;
		break;
	case CURSOR_INF:
		cursor = GLFW_CURSOR_DISABLED;
		break;
	}

	glfwSetInputMode(win, GLFW_CURSOR, cursor);
	mouse_dirty = 0.f;
}

void rubbish_run(
	const rubbish_cfg cfg,
	void (*init)(void),
	void (*update)(void),
	void (*update_fixed)(void)
) {
	assert(init);
	assert(update);

#ifdef DEBUG_FPE
	feenableexcept(FE_DIVBYZERO | FE_INVALID);
	signal(SIGFPE, fpehandler);
#endif
	if (!glfwInit())
		panic();

	GLFWmonitor *mon = glfwGetPrimaryMonitor();
	assert(mon);

	const GLFWvidmode *mode = glfwGetVideoMode(mon);
	assert(mode);

	win = glfwCreateWindow(
		mode->width,
		mode->height,
		"jam21",
		mon,
		NULL
	);

	assert(win);
	glfwMakeContextCurrent(win);
	set_cursor(cfg.cursor);

	if (gl3wInit())
		panic();

#ifdef DEBUG
	glEnable(GL_DEBUG_OUTPUT);
	glDebugMessageCallback(log_gl, 0);
#endif

	shaders_init();

	glViewport(0, 0, mode->width, mode->height);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);

	frame.w = mode->width;
	frame.h = mode->height;
	cam.asp = frame.w / frame.h;

	static abuf *verts;
	ABUF_MK_MB(verts, float, 1, ALLOC_VMEM);
	ABUF_MK_MB(render.meshes, mesh, 1, ALLOC_VMEM);
	ABUF_MK_MB(render.lines, line, 1, ALLOC_VMEM);

	GLuint vao, vbo;
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(
		GL_ARRAY_BUFFER,
		sizeof(float) * verts->cap,
		NULL,
		GL_DYNAMIC_DRAW
	);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * 3, 0);
	glEnableVertexAttribArray(0);
	glEnable(GL_DEPTH_TEST);

#ifdef RUBBISH_IMGUI
	imgui = igCreateContext(NULL);
	imgui_io = igGetIO();
	{
		const char *v = "#version 330 core";
		ImGui_ImplGlfw_InitForOpenGL(win, 1);
		ImGui_ImplOpenGL3_Init(v);
		igStyleColorsDark(NULL);
		imgui_io->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	}
#endif
	init();
	glfwSetTime(0.0);
	double t_last = 0.f;

	while (!glfwWindowShouldClose(win)) {
		double t = glfwGetTime();
		double dt = t - t_last;
		t_last = t;

		frame_clear();
#ifdef RUBBISH_IMGUI
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		igNewFrame();
#endif
		if (_time_tick(&t, &dt)) {
			// ... //
		}

		if (update_fixed) {
			while (_time_step())
				update_fixed();
		}

		update();

		const m4 vp = cam3_conv(cam, 1);
		glUniformMatrix4fv(unif.vp, 1, GL_FALSE, vp.s);
		glClearColor(render.col.x, render.col.y, render.col.z, 1.f);
		glUniform3fv(unif.clear, 1, render.col.s);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glCullFace(GL_BACK);

		GLintptr off = 0;
		GLsizei n = 0;

		ABUF_FOREACH(render.meshes, mesh) {
			if (mesh->wire | cfg.wireframe) {
				glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
				glDisable(GL_CULL_FACE);
			} else {
				glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
				glEnable(GL_CULL_FACE);
			}

			glBufferSubData(
				GL_ARRAY_BUFFER,
				off,
				mesh->size,
				mesh->pts
			);

			glUniform3fv(unif.col, 1, mesh->col.s);
			glUniformMatrix4fv(
				unif.model,
				1,
				GL_FALSE,
				t3_to_m4(mesh->trs).s
			);

			glDrawArrays(GL_TRIANGLES, n, mesh->n * 3);

			off += mesh->size;
			n += mesh->n * 3;
		}

		ABUF_FOREACH(render.lines, line) {
			glBufferSubData(
				GL_ARRAY_BUFFER,
				off,
				SIZE_LINE,
				(void*)line
			);

			glUniform3fv(unif.col, 1, line->col.s);
			glLineWidth(3.f);
			glDrawArrays(GL_LINES, n, 2);

			off += SIZE_LINE;
			n += 2;
		}
#ifdef RUBBISH_IMGUI
		igRender();
		ImGui_ImplOpenGL3_RenderDrawData(igGetDrawData());
#endif
		glfwSwapBuffers(win);
		glfwPollEvents();

		for (int e = 0; e < GLFW_KEY_LAST; ++e)
			key_inc(e);
		for (int e = 0; e < GLFW_MOUSE_BUTTON_LAST; ++e)
			btn_inc(e);

		double mx, my;
		glfwGetCursorPos(win, &mx, &my);

		ff pos = { mx, my * -1.f };
		mouse.pos = pos;

		static ff mouse_last;
		mouse_last = mixff(mouse.pos, mouse_last, mouse_dirty);
		mouse_dirty = 1.f;

		mouse.delta = subff(pos, mouse_last);
		mouse_last = pos;
	}

#ifdef RUBBISH_IMGUI
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	igDestroyContext(imgui);
#endif
	glfwTerminate();
}
