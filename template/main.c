#include "rubbish/bin.h"
#include "rubbish/mesh.h"
#include "acg/rand.h"
#include "acg/sys.h"

static void init()
{
}

static void update()
{
	render.col = V3_ZERO; // Set clear color (optional)

	/* UI via cimgui */

	igShowDemoWindow(NULL); // UI

	/* Input, mesh rendering */

	if (!key_held(GLFW_KEY_SPACE)) {
		draw.trs.pos = (v3) { 0.f, 0.f, 2.f };
		draw.trs.scale = 1.5f;
		draw_ico((ico) { 1.0f });
	}

	/* Set camera transform */

	cam.pos = V3_BCK;
	cam.rot = qt_axis_angle(V3_FWD, .05f * sinf(_time.el.game));
	cam.fov = 45.f;

	/* Frame data example */

	static float t_last;
	if (_time.el.real - t_last > 1.f) {
		printf("width =%f\n", frame.w);
		printf("height=%f\n", frame.h);
		printf(" t=%f\n", _time.el.real);
		printf("dt=%f fps=%f\n", _time.dt.real, 1.f / _time.dt.real);
		printf("dt_fixed=%f fps_fixed=%f\n", FIXED_DT, 1.f / FIXED_DT);
		printf("\n");
		t_last = _time.el.real;
	}
}

static void update_fixed()
{
	// ...
}

int main()
{
	rubbish_cfg cfg = {
		.flags = RUBBISH_CFG_WIRE | RUBBISH_CFG_AA,
		.line_width = 2.f,
		.vert = {}, // Fill from vert.h for custom vertex shader
		.frag = {}, // Fill from frag.h for custom fragment shader
	};

	rubbish_run(cfg, init, update, update_fixed);
}
