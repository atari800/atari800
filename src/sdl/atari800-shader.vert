
#version 410 core
in vec3 aPos;
in vec2 aTexCoord;
uniform mat4 u_vp_matrix;

out vec2 TexCoord; 

void main() {
	gl_Position = u_vp_matrix * vec4(aPos, 1.0);
	TexCoord = aTexCoord;
}
