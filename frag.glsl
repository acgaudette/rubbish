#version 330 core

uniform vec3 col;
uniform vec3 col_clear;
out vec4 final;

void main()
{
	final = vec4(col.rgb, 1);
}
