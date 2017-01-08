#version 120

uniform vec4 u_Color;

varying vec2 v_Tex;
uniform sampler2D u_TexSampler;

void main(){
    float fGreyColor = texture2D(u_TexSampler, v_Tex).r;
    gl_FragColor = u_Color * vec4(vec3(fGreyColor), 1);
}
