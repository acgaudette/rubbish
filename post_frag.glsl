#version 330 core

uniform sampler2D img;

in  vec2 uv;
out vec4 final;

void main()
{
	vec4 col = texture(img, uv);
	final = vec4(col.rgb, 1);
}
