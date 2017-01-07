#version 120

uniform mat4 u_PMV;

attribute vec3 a_Pos;
attribute vec2 a_Tex;
varying vec2 v_Tex;

void main(){
	gl_Position = u_PMV * vec4(a_Pos, 1.0);
	v_Tex = a_Tex;
}
