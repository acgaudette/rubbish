#version 330 core

out vec2 uv;

void main()
{
	uv = vec2(
		(gl_VertexID << 1) & 2,
		 gl_VertexID & 2
	);

	gl_Position = vec4(
		2 * uv - 1,
		0,
		1
	);
}
