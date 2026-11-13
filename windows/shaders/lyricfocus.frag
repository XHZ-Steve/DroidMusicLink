#version 440
layout(location=0) in vec2 qt_TexCoord0;
layout(location=0) out vec4 fragColor;
layout(std140,binding=0) uniform buf { mat4 qt_Matrix; float qt_Opacity; vec4 fading; vec4 focusWindow; };
layout(binding=1) uniform sampler2D source;
void main(){
 float top=fading.x>0.0?smoothstep(0.0,fading.x,qt_TexCoord0.y):1.0;
 float bottom=fading.y>0.0?smoothstep(0.0,fading.y,1.0-qt_TexCoord0.y):1.0;
 float mask=mix(1.0,top*bottom,clamp(fading.z,0.0,1.0));
 float focusTop=smoothstep(focusWindow.x-focusWindow.z,focusWindow.x,qt_TexCoord0.y);
 float focusBottom=1.0-smoothstep(focusWindow.y,focusWindow.y+focusWindow.z,qt_TexCoord0.y);
 mask*=mix(1.0,focusTop*focusBottom,focusWindow.w);
 fragColor=texture(source,qt_TexCoord0)*mask*qt_Opacity;
}
