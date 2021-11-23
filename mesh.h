enum mesh_flags {
	  MESH_NONE = 0x0
	, MESH_TMP  = 0x1
};

struct verts {
	u32  n;
	fff *v;
};

static struct verts verts_new(const u32 n, int tmp)
{
	struct verts result = { .n = n };
	const u32 size = result.n * sizeof(fff);
	result.v = tmp ? bump(size) : alloc(size, ALLOC_SYS);
	assert(result.v);
	return result;
}

static mesh verts_to_mesh(const struct verts verts)
{
	assert(!(verts.n % 3));
	mesh result = mesh_new(verts.n / 3, verts.v);
	return result;
}

typedef struct {
	v3 pts[4];
	enum mesh_flags flags;
} quad;

static struct verts quad_verts(const quad quad)
{
	const u32 n = 6;
	struct verts result = verts_new(n, quad.flags & MESH_TMP);

	const v3 pts[] = {
		quad.pts[0],
		quad.pts[1],
		quad.pts[2],
		quad.pts[2],
		quad.pts[3],
		quad.pts[0],
	};

	memcpy(result.v, pts, n * sizeof(fff));
	return result;
}

static mesh quad_to_mesh(const quad quad)
{
	return verts_to_mesh(quad_verts(quad));
}

typedef struct {
	ff anc;
	ff ext;
	v4 rot;
	enum mesh_flags flags;
} bill;

static struct verts bill_verts(const bill bill)
{
	const ff anchor = mulff(addff(bill.anc, V2_ONE), .5f);
	const float xl = lerpf(0.f, 1.f, -anchor.x);
	const float xr = lerpf(1.f, 0.f,  anchor.x);
	const float yl = lerpf(0.f, 1.f, -anchor.y);
	const float yr = lerpf(1.f, 0.f,  anchor.y);

	quad quad = {
		.pts = {
			{ xl, yl, 0.f },
			{ xl, yr, 0.f },
			{ xr, yr, 0.f },
			{ xr, yl, 0.f },
		},
		.flags = bill.flags,
	};

	for (u32 i = 0; i < 4; ++i) {
		quad.pts[i] = schurfff(quad.pts[i], v3_padxy(bill.ext, 0.f));
		quad.pts[i] = qt_app(bill.rot, quad.pts[i]);
	}

	return quad_verts(quad);
}

static mesh bill_to_mesh(const bill bill)
{
	return verts_to_mesh(bill_verts(bill));
}

typedef struct {
	v3 ext;
	v3 anc;
	enum mesh_flags flags;
} cuboid;

static struct verts cuboid_verts(const cuboid cuboid)
{
	const u32 n = 6 * 2 * 3;
	struct verts result = verts_new(n, cuboid.flags & MESH_TMP);
	const v3 anchor = v3_mul(v3_add(cuboid.anc, V3_ONE), .5f);

	const float xl = lerpf(0.f, cuboid.ext.x, -anchor.x);
	const float xr = lerpf(cuboid.ext.x, 0.f,  anchor.x);

	const float yl = lerpf(0.f, cuboid.ext.y, -anchor.y);
	const float yr = lerpf(cuboid.ext.y, 0.f,  anchor.y);

	const float zl = lerpf(0.f, cuboid.ext.z, -anchor.z);
	const float zr = lerpf(cuboid.ext.z, 0.f,  anchor.z);

	quad faces[] = {
		{
			(v3) { xl, yl, zl },
			(v3) { xl, yr, zl },
			(v3) { xr, yr, zl },
			(v3) { xr, yl, zl },
		}, {
			(v3) { xl, yr, zl },
			(v3) { xl, yr, zr },
			(v3) { xr, yr, zr },
			(v3) { xr, yr, zl },
		}, {
			(v3) { xl, yr, zr },
			(v3) { xl, yl, zr },
			(v3) { xr, yl, zr },
			(v3) { xr, yr, zr },
		}, {
			(v3) { xl, yl, zr },
			(v3) { xl, yl, zl },
			(v3) { xr, yl, zl },
			(v3) { xr, yl, zr },
		}, {
			(v3) { xr, yl, zl },
			(v3) { xr, yr, zl },
			(v3) { xr, yr, zr },
			(v3) { xr, yl, zr },
		}, {
			(v3) { xl, yl, zr },
			(v3) { xl, yr, zr },
			(v3) { xl, yr, zl },
			(v3) { xl, yl, zl },
		}
	};

	u32 i = 0;
	for (u32 j = 0; j < sizeof(faces) / sizeof(quad); ++j) {
		struct verts face = quad_verts(faces[j]);
		memcpy(result.v + i, face.v, 6 * sizeof(v3));
		free(face.v);
		i += 6;
	}

	return result;
}

static mesh cuboid_to_mesh(const cuboid cuboid)
{
	return verts_to_mesh(cuboid_verts(cuboid));
}

typedef struct {
	float scale;
	enum mesh_flags flags;
} ico;

static struct verts ico_verts(const ico ico)
{
	const u32 n = 20 * 3;
	struct verts result = verts_new(n, ico.flags & MESH_TMP);

	const float phi = .5f * (1.f + sqrt(5.f));
	const float l = .5f * ico.scale;
	const float s = .5f * ico.scale / phi;

	const v3 pts[] = {
		{  -s,  0.f,  -l }, { 0.f,   l,  -s }, {   s, 0.f,  -l }, // Front
		{   s,  0.f,  -l }, { 0.f,  -l,  -s }, {  -s, 0.f,  -l },
		{ 0.f,    l,  -s }, {  -l,   s, 0.f }, { 0.f,   l,   s }, // Top
		{ 0.f,    l,   s }, {   l,   s, 0.f }, { 0.f,   l,  -s },
		{   s,  0.f,   l }, { 0.f,   l,   s }, {  -s, 0.f,   l }, // Back
		{  -s,  0.f,   l }, { 0.f,  -l,   s }, {   s, 0.f,   l },
		{ 0.f,   -l,  -s }, {   l,  -s, 0.f }, { 0.f,  -l,   s }, // Bot
		{ 0.f,   -l,   s }, {  -l,  -s, 0.f }, { 0.f,  -l,  -s },
		{   l,    s, 0.f }, {   s, 0.f,   l }, {   l,  -s, 0.f }, // Right
		{   l,   -s, 0.f }, {   s, 0.f,  -l }, {   l,   s, 0.f },
		{  -l,    s, 0.f }, {  -s, 0.f,  -l }, {  -l,  -s, 0.f }, // Left
		{  -l,   -s, 0.f }, {  -s, 0.f,   l }, {  -l,   s, 0.f },
		{ 0.f,    l,  -s }, {   l,   s, 0.f }, {   s, 0.f,  -l }, // Corners
		{ 0.f,    l,  -s }, {  -s, 0.f,  -l }, {  -l,   s, 0.f },
		{ 0.f,    l,   s }, {   s, 0.f,   l }, {   l,   s, 0.f },
		{ 0.f,    l,   s }, {  -l,   s, 0.f }, {  -s, 0.f,   l },
		{ 0.f,   -l,  -s }, {   s, 0.f,  -l }, {   l,  -s, 0.f },
		{ 0.f,   -l,   s }, {   l,  -s, 0.f }, {   s, 0.f,   l },
		{ 0.f,   -l,  -s }, {  -l,  -s, 0.f }, {  -s, 0.f,  -l },
		{ 0.f,   -l,   s }, {  -s, 0.f,   l }, {  -l,  -s, 0.f },
	};

	memcpy(result.v, pts, sizeof(pts));
	return result;
}

static mesh ico_to_mesh(const ico ico)
{
	return verts_to_mesh(ico_verts(ico));
}

/* "Immediate" mesh rendering */

static int mesh_wire;
static fff mesh_col = V3_ONE;
static fff mesh_vfx = V3_ZERO;
static ttt mesh_trs = T3_ID;

static void mesh_draw(mesh mesh)
{
	mesh.trs  = mesh_trs;
	mesh.col  = mesh_col;
	mesh.vfx  = mesh_vfx;
	mesh.wire = mesh_wire;
	meshes_push(&mesh);
}

static void mesh_quad(quad quad)
{
	quad.flags |= MESH_TMP;
	mesh mesh = quad_to_mesh(quad);
	mesh_draw(mesh);
}

static void mesh_bill(bill bill)
{
	bill.flags |= MESH_TMP;
	mesh mesh = bill_to_mesh(bill);
	mesh_draw(mesh);
}

static void mesh_cuboid(cuboid cuboid)
{
	cuboid.flags |= MESH_TMP;
	mesh mesh = cuboid_to_mesh(cuboid);
	mesh_draw(mesh);
}

static void mesh_ico(ico ico)
{
	ico.flags |= MESH_TMP;
	mesh mesh = ico_to_mesh(ico);
	mesh_draw(mesh);
}
