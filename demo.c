#include "rubbish/bin.h"
#include "rubbish/mesh.h"
#include "acg/rand.h"
#include "acg/sys.h"

static void init()
{
}

static void update()
{
	igShowDemoWindow(NULL);

	const icosph ico;
	mesh mesh = icosph_conv(ico);
	mesh.trs.pos = (v3) { 0.f, 0.f, 2.f };
	meshes_push(&mesh);
	cam.pos = (v3) { 0.f, 0.f, -1.f };
}

static void update_fixed()
{
	// ...
}

int main()
{
	rubbish_cfg cfg = { .wireframe = 1, .aa = 1 };
	rubbish_run(cfg, init, update, update_fixed);
}
