#version 120

uniform vec4 u_Color;

varying vec2 v_Tex;
uniform sampler2D u_TexSampler;

void main(){
	gl_FragColor = u_Color * texture2D(u_TexSampler, v_Tex);
}
