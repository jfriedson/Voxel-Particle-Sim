#version 460

in vec2 texCoord;

out vec4 color;

uniform sampler2D rtTexture;


void main() {
	color = vec4(texture(rtTexture, texCoord).xyz, 1.f);
}
