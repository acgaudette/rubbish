#version 330 core

uniform mat4 vp;
uniform mat4 trs;

layout (location = 0) in vec3 pos;

void main()
{
	gl_Position = vp * (trs * vec4(pos, 1));
}
