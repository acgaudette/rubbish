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
	GLint tex;
} unif;

void lines_push(const fff a, const fff b, const fff col)
{
	*(line*)abuf_push(render.lines) = (line) { a, b, col };
}

mesh mesh_new(const u32 n, void *mem)
{
	u32 size = n * 3 * sizeof(fff);
	fff *pts = mem ?: malloc(size);
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

void mesh_free(mesh *mesh)
{
	free(mesh->pts);
}

void meshes_push(const mesh *in)
{
	if (!in->pts)
		return;
	*(mesh*)abuf_push(render.meshes) = *in;
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
#include "post_vert.h"
#include "post_frag.h"

#define SHADER_INIT(T, HANDLE)           \
	shader_init(                     \
		GL_ ## T ## _SHADER,     \
		HANDLE ## _glsl_len,     \
		(char*) HANDLE ## _glsl, \
		#HANDLE)

GLuint shader_init(
	const GLenum type,
	const GLint len,
	const GLchar *buf,
	const char *handle
) {
	switch (type) {
	case GL_VERTEX_SHADER:
		break;
	case GL_FRAGMENT_SHADER:
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
		fprintf(stderr, "[compile] %s shader:\n%s\n", handle, log);
		panic();
	}

	return sh;
}

struct shaders {
	GLuint base;
	GLuint post;
};

static void shaders_link(
	const GLuint prog,
	const GLuint vert,
	const GLuint frag
) {
	static char log[128];
	int result;

	glAttachShader(prog, vert);
	glAttachShader(prog, frag);
	glLinkProgram (prog);

	memset(log, 0, 128);
	glGetProgramiv(prog, GL_LINK_STATUS, &result);
	if (!result) {
		glGetProgramInfoLog(prog, 128, NULL, log);
		fprintf(stderr, "[link] shaders:\n%s\n", log);
		panic();
	}
}

static struct shaders shaders_init(
	const int aa,
	const struct shader *vert_in,
	const struct shader *frag_in
) {
	struct shaders shaders = {
		.base = glCreateProgram(),
	};

	GLuint vert, frag;

	if (vert_in) {
		vert = shader_init(
			GL_VERTEX_SHADER,
			vert_in->n,
			vert_in->raw,
			"vert_user"
		);
	} else vert = SHADER_INIT(VERTEX, vert);
	if (frag_in) {
		frag = shader_init(
			GL_FRAGMENT_SHADER,
			frag_in->n,
			frag_in->raw,
			"frag_user"
		);
	} else frag = SHADER_INIT(FRAGMENT, frag);

	shaders_link(shaders.base, vert, frag);
	unif.vp    = glGetUniformLocation(shaders.base, "vp");
	unif.model = glGetUniformLocation(shaders.base, "trs");
	unif.col   = glGetUniformLocation(shaders.base, "col");
	unif.clear = glGetUniformLocation(shaders.base, "col_clear");

	glDeleteShader(vert);
	glDeleteShader(frag);

	if (aa)
		return shaders;

	shaders.post = glCreateProgram();
	vert = SHADER_INIT(VERTEX,   post_vert);
	frag = SHADER_INIT(FRAGMENT, post_frag);

	shaders_link(shaders.post, vert, frag);
	unif.tex = glGetUniformLocation(shaders.post, "img");

	glDeleteShader(vert);
	glDeleteShader(frag);

	return shaders;
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

	const int aa = cfg.flags & RUBBISH_CFG_AA;
	assert(!(aa && cfg.crush));

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

	struct shaders shaders = shaders_init(
		aa,
		cfg.vert.n ? &cfg.vert : NULL,
		cfg.frag.n ? &cfg.frag : NULL
	);

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

	GLuint vao_empty;
	glGenVertexArrays(1, &vao_empty);

	GLuint fb;
	glGenFramebuffers(1, &fb);
	glBindFramebuffer(GL_FRAMEBUFFER, fb);

	const u8 div = 1 << cfg.crush;

	GLuint rtex;
	glGenTextures(1, &rtex);
	if (aa) {
		glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, rtex);
		glTexImage2DMultisample(
			GL_TEXTURE_2D_MULTISAMPLE,
			4,
			GL_RGB,
			frame.w,
			frame.h,
			1
		);
	} else {
		glBindTexture(GL_TEXTURE_2D, rtex);
		glTexImage2D(
			GL_TEXTURE_2D,
			0,
			GL_RGB,
			frame.w / div,
			frame.h / div,
			0,
			GL_RGB,
			GL_UNSIGNED_BYTE,
			0
		);
	}

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

	GLuint rtex_depth;
	glGenRenderbuffers(1, &rtex_depth);
	glBindRenderbuffer(GL_RENDERBUFFER, rtex_depth);
	if (aa) {
		glRenderbufferStorageMultisample(
			GL_RENDERBUFFER,
			4,
			GL_DEPTH_COMPONENT,
			frame.w,
			frame.h
		);
	} else {
		glRenderbufferStorage(
			GL_RENDERBUFFER,
			GL_DEPTH_COMPONENT,
			frame.w / div,
			frame.h / div
		);
	}

	glFramebufferRenderbuffer(
		GL_FRAMEBUFFER,
		GL_DEPTH_ATTACHMENT,
		GL_RENDERBUFFER,
		rtex_depth
	);

	if (aa) {
		glFramebufferTexture2D(
			GL_FRAMEBUFFER,
			GL_COLOR_ATTACHMENT0,
			GL_TEXTURE_2D_MULTISAMPLE,
			rtex,
			0
		);
	}

	else glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, rtex, 0);

	GLenum buf[] = { GL_COLOR_ATTACHMENT0 };
	glDrawBuffers(1, buf);

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

		abuf_clear(render.meshes);
		if (!(cfg.flags & RUBBISH_CFG_LINE_PERSIST))
			abuf_clear(render.lines);
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

		glUseProgram(shaders.base);
		glBindVertexArray(vao);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBindFramebuffer(GL_FRAMEBUFFER, fb);
		glViewport(0, 0, frame.w / div, frame.h / div);

		const m4 vp = cam3_conv(cam, 1);
		glUniformMatrix4fv(unif.vp, 1, GL_FALSE, vp.s);
		glClearColor(render.col.x, render.col.y, render.col.z, 1.f);
		glUniform3fv(unif.clear, 1, render.col.s);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glCullFace(GL_BACK);

		GLintptr off = 0;
		GLsizei n = 0;

		ABUF_FOREACH(render.meshes, mesh) {
			if (mesh->wire | (cfg.flags & RUBBISH_CFG_WIRE)) {
				glLineWidth(cfg.line_width ?: 1.f);
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
			glUniformMatrix4fv(
				unif.model,
				1,
				GL_FALSE,
				M4_ID.s
			);

			glLineWidth(cfg.line_width ?: 1.f);
			glDrawArrays(GL_LINES, n, 2);

			off += SIZE_LINE;
			n += 2;
		}

		if (aa) {
			glBindFramebuffer(GL_READ_FRAMEBUFFER, fb);
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
			glBlitFramebuffer(
				0,
				0,
				frame.w,
				frame.h,
				0,
				0,
				frame.w,
				frame.h,
				GL_COLOR_BUFFER_BIT,
				GL_NEAREST
			);
		} else {
			glUseProgram(shaders.post);
			glBindVertexArray(vao_empty);
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			glViewport(0, 0, frame.w, frame.h);
			glClearColor(render.col.x, render.col.y, render.col.z, 1.f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
			glDrawArrays(GL_TRIANGLES, 0, 6);
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
