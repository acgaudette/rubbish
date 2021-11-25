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
	result.v = tmp ? bump_alloc(size) : alloc(size, ALLOC_SYS);
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
	float size;
	ff anc;
	u32 res;
	int border;
	float (*fn)(const ff);
	enum mesh_flags flags;
} heightmap;

static struct verts heightmap_verts(const heightmap map)
{
	assert(map.fn);
	assert(map.res);
	assert(map.size);

	const u32 n = 6 * map.res * map.res
		+ 6 * map.res * 4 * map.border;
	struct verts result = verts_new(n, map.flags & MESH_TMP);

	const float dist = map.size / map.res;
	const ff off = mulff(subff(map.anc, (ff) { .5f, .5f }), map.size);

	u32 k = 0;
	for (u32 i = 0; i < map.res; ++i) {
		for (u32 j = 0; j < map.res; ++j) {
			const float x0 = (i + 0) * dist;
			const float x1 = (i + 1) * dist;
			const float y0 = (j + 0) * dist;
			const float y1 = (j + 1) * dist;

			const float x0y0 = map.fn((ff) { x0, y0 });
			const float x0y1 = map.fn((ff) { x0, y1 });
			const float x1y1 = map.fn((ff) { x1, y1 });
			const float x1y0 = map.fn((ff) { x1, y0 });

			quad face = {
				(v3) { x0, x0y0, y0 },
				(v3) { x0, x0y1, y1 },
				(v3) { x1, x1y1, y1 },
				(v3) { x1, x1y0, y0 },
			};

			for (u32 l = 0; l < 4; ++l)
				v3_addeq(face.pts + l, v3_padxz(off, 0.f));

			struct verts out = quad_verts(face);
			memcpy(result.v + k, out.v, 6 * sizeof(v3));
			free(out.v);
			k += 6;
		}
	}

	if (!map.border)
		return result;

	const float limit = map.res * dist;

	for (u32 i = 0; i < map.res; ++i) {
		const float a = map.fn((ff) { i * dist,        0.f });
		const float b = map.fn((ff) { i * dist + dist, 0.f });
		const float c = map.fn((ff) { i * dist,        limit });
		const float d = map.fn((ff) { i * dist + dist, limit });
		const float e = map.fn((ff) { 0.f,        i * dist });
		const float f = map.fn((ff) { 0.f, i * dist + dist });
		const float g = map.fn((ff) { limit,        i * dist });
		const float h = map.fn((ff) { limit, i * dist + dist });

		quad faces[] = {
			{
				(v3) { i * dist,        0.f, 0.f },
				(v3) { i * dist,          a, 0.f },
				(v3) { i * dist + dist,   b, 0.f },
				(v3) { i * dist + dist, 0.f, 0.f },
			}, {
				(v3) { i * dist + dist, 0.f, limit },
				(v3) { i * dist + dist,   d, limit },
				(v3) { i * dist,          c, limit },
				(v3) { i * dist,        0.f, limit },
			}, {
				(v3) { 0.f, 0.f, i * dist + dist },
				(v3) { 0.f,   f, i * dist + dist },
				(v3) { 0.f,   e, i * dist },
				(v3) { 0.f, 0.f, i * dist },
			}, {
				(v3) { limit, 0.f, i * dist },
				(v3) { limit,   g, i * dist },
				(v3) { limit,   h, i * dist + dist },
				(v3) { limit, 0.f, i * dist + dist },
			}
		};

		for (u32 j = 0; j < sizeof(faces) / sizeof(quad); ++j) {
			for (u32 l = 0; l < 4; ++l)
				v3_addeq(faces[j].pts + l, v3_padxz(off, 0.f));
			struct verts face = quad_verts(faces[j]);
			memcpy(result.v + k, face.v, 6 * sizeof(v3));
			free(face.v);
			k += 6;
		}
	}

	return result;
}

static mesh heightmap_to_mesh(const heightmap map)
{
	return verts_to_mesh(heightmap_verts(map));
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

static mesh draw = {
	.col = V3_ONE,
	.trs = T3_ID,
};

static void mesh_draw(mesh mesh)
{
	mesh.trs = draw.trs;
	mesh.vfx = draw.vfx;

	mesh.wire = draw.wire;
	for (u32 i = 0; i < 4; ++i)
		mesh.cols[i] = draw.cols[i];

	meshes_push(&mesh);
}

static void draw_quad(quad quad)
{
	quad.flags |= MESH_TMP;
	mesh mesh = quad_to_mesh(quad);
	mesh_draw(mesh);
}

static void draw_heightmap(heightmap map)
{
	map.flags |= MESH_TMP;
	mesh mesh = heightmap_to_mesh(map);
	mesh_draw(mesh);
}

static void draw_bill(bill bill)
{
	bill.flags |= MESH_TMP;
	mesh mesh = bill_to_mesh(bill);
	mesh_draw(mesh);
}

static void draw_cuboid(cuboid cuboid)
{
	cuboid.flags |= MESH_TMP;
	mesh mesh = cuboid_to_mesh(cuboid);
	mesh_draw(mesh);
}

static void draw_ico(ico ico)
{
	ico.flags |= MESH_TMP;
	mesh mesh = ico_to_mesh(ico);
	mesh_draw(mesh);
}
