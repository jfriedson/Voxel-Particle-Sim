#version 460

out vec2 texCoord;


void main() {
	vec2 position = vec2(gl_VertexID % 2, gl_VertexID / 2) * 4.f - 1.f;
	texCoord = (position + 1.f) * 0.5f;

	gl_Position = vec4(position, 0.f, 1.f);
}
