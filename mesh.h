typedef struct {
	ff   ext;
	fff  pos;
	ffff rot;
} quad;

static u32 quad_sprint(quad quad, mesh *out, u32 off)
{
	const u32 n = 3 * 2;

	fff pts[] = {
		{ 0, 0, 0 },
		{ 0, 0, 1 },
		{ 1, 0, 1 }, // Tri
		{ 1, 0, 1 },
		{ 1, 0, 0 },
		{ 0, 0, 0 } // Tri
	};

	for (u32 i = 0; i < n; ++i) {
		pts[i] = schurfff(
			pts[i],
			(fff) { quad.ext.x, 0.f, quad.ext.y }
		);

		pts[i] = qt_app(quad.rot, pts[i]);
		pts[i] = addfff(pts[i], quad.pos);
	}

	memcpy(out->pts + off * 3, &pts, n * sizeof(fff));
	return 2;
}

typedef struct {
	fff ext;
	fff pos;
	fff anc;
} cube;

static mesh cube_conv(cube cube)
{
	mesh result = mesh_new(6 * 2);
	u32 off = 0;
	fff anchor = schurfff(cube.ext, v3_neg(cube.anc));

	off += quad_sprint( // Bot
		(quad) {
			.ext = { -cube.ext.x, cube.ext.z },
			.pos = addfff(cube.pos, anchor),
			.rot = qt_vecs(V3_FWD, V3_DN),
		},
		&result,
		off
	);

	off += quad_sprint( // Top
		(quad) {
			.ext = { cube.ext.x, cube.ext.z },
			.pos = addfff(
				cube.pos,
				addfff(
					(fff) { 0, cube.ext.y, 0 },
					anchor
				)
			),
			.rot = qt_vecs(V3_FWD, V3_UP),
		},
		&result,
		off
	);

	off += quad_sprint( // Back
		(quad) {
			.ext = { cube.ext.x, cube.ext.y },
			.pos = addfff(cube.pos, anchor),
			.rot = qt_vecs(V3_UP, V3_BCK),
		},
		&result,
		off
	);

	off += quad_sprint( // Front
		(quad) {
			.ext = { -cube.ext.x, cube.ext.y },
			.pos = addfff(
				cube.pos,
				addfff(
					(fff) { 0, 0, cube.ext.z },
					anchor
				)
			),
			.rot = qt_vecs(V3_UP, V3_FWD),
		},
		&result,
		off
	);

	off += quad_sprint( // Left
		(quad) {
			.ext = { cube.ext.y, cube.ext.z },
			.pos = addfff(cube.pos, anchor),
			.rot = qt_vecs(V3_FWD, V3_LFT),
		},
		&result,
		off
	);

	off += quad_sprint( // Right
		(quad) {
			.ext = { -cube.ext.y, cube.ext.z },
			.pos = addfff(
				cube.pos,
				addfff(
					(fff) { cube.ext.x, 0, 0 },
					anchor
				)
			),
			.rot = qt_vecs(V3_FWD, V3_RT),
		},
		&result,
		off
	);

	return result;
}

typedef struct {
} icosph;

// schneide.blog/2016/07/15/generating-an-icosphere-in-c++
static mesh icosph_conv(icosph ico)
{
	mesh result = mesh_new(20);

	const float X = .525731112119133606f;
	const float Z = .850650808352039932f;
	const float N = 0.f;

	static const v3 verts[] = {
		{ -X, N, Z }, {  X, N,  Z }, { -X,  N, -Z }, {  X,  N, -Z },
		{  N, Z, X }, {  N, Z, -X }, {  N, -Z,  X }, {  N, -Z, -X },
		{  Z, X, N }, { -Z, X,  N }, {  Z, -X,  N }, { -Z, -X,  N },
	};

	const v3 all[] = {
		verts[0],  verts[4],  verts[1],
		verts[0],  verts[9],  verts[4],
		verts[9],  verts[5],  verts[4],
		verts[4],  verts[5],  verts[8],
		verts[4],  verts[8],  verts[1],
		verts[8],  verts[10], verts[1],
		verts[8],  verts[3],  verts[10],
		verts[5],  verts[3],  verts[8],
		verts[5],  verts[2],  verts[3],
		verts[2],  verts[7],  verts[3],
		verts[7],  verts[10], verts[3],
		verts[7],  verts[6],  verts[10],
		verts[7],  verts[11], verts[6],
		verts[11], verts[0],  verts[6],
		verts[0],  verts[1],  verts[6],
		verts[6],  verts[1],  verts[10],
		verts[9],  verts[0],  verts[11],
		verts[9],  verts[11], verts[2],
		verts[9],  verts[2],  verts[5],
		verts[7],  verts[2],  verts[11],
	};

	memcpy(result.pts, &all, 20 * 3 * sizeof(v3));
	return result;
}

typedef struct {
	v3 ext;
} cubef;

static mesh cubef_conv(cubef cube)
{
	mesh result = mesh_new(6 * 2);

	const float xl = -cube.ext.x * .5f;
	const float xr =  cube.ext.x * .5f;

	const float yl = -cube.ext.y * .5f;
	const float yr =  cube.ext.y * .5f;

	const float zl = -cube.ext.z * .5f;
	const float zr =  cube.ext.z * .5f;

	const v3 verts[] = {
		{ xl, yl, zl }, { xl, yl, zr }, { xr, yl, zr }, { xr, yl, zl }, // Bottom
		{ xl, yr, zl }, { xl, yr, zr }, { xr, yr, zr }, { xr, yr, zl }, // Top (rev)
	};

	const v3 all[] = {
		verts[0], verts[1], verts[2],
		verts[2], verts[3], verts[0], // Bottom
		verts[6], verts[5], verts[4],
		verts[4], verts[7], verts[6], // Top
		verts[5], verts[1], verts[0],
		verts[0], verts[4], verts[5], // Left
		verts[7], verts[3], verts[2],
		verts[2], verts[6], verts[7], // Right
		verts[0], verts[3], verts[7],
		verts[7], verts[4], verts[0], // Back
		verts[6], verts[2], verts[1],
		verts[1], verts[5], verts[6], // Front
	};

	memcpy(result.pts, &all, sizeof(all));
	return result;
}
