#version 330 core

uniform vec3 col;
out vec4 final;

void main()
{
	final = vec4(col.rgb, 1);
}
