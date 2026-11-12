#version 440
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec4 primaryColor;
    vec4 secondaryColor;
    vec4 accentColor;
    vec4 deepColor;
    vec4 tuning;
    vec4 atmosphere;
    vec4 geometry;
    float phase;
};
const vec3 luma=vec3(.2126,.7152,.0722);
vec3 linearColor(vec3 c) { return pow(max(c,vec3(0)),vec3(2.2)); }
vec3 vivid(vec3 c) {
    float y=dot(c,luma);
    float chroma=max(c.r,max(c.g,c.b))-min(c.r,min(c.g,c.b));
    float amount=max(0.,tuning.z+tuning.w*(1.-chroma));
    return clamp(mix(vec3(y),c,amount),0.,1.);
}
float region(vec2 p,vec2 center,vec2 stretch,float radius) {
    vec2 delta=(p-center)*stretch;
    return exp(-dot(delta,delta)/max(.015,radius*radius));
}
void main() {
    vec2 uv=qt_TexCoord0;
    float aspect=max(geometry.x,1.)/max(geometry.y,1.);
    vec2 metric=vec2(sqrt(aspect),inversesqrt(aspect));
    float radius=.42*atmosphere.y;
    float t=phase*.055;
    vec2 drift=vec2(sin(t),cos(t*.73))*.045;
    float a=region(uv,vec2(.14,.25)+drift,metric,radius);
    float b=region(uv,vec2(.93,.80)-drift,metric,radius*.95);
    float c=region(uv,vec2(.68,.12)+drift.yx*.6,metric,radius*.65)*.52;
    // Local colour ownership prevents opposite hues becoming a flat grey field.
    vec3 weights=pow(vec3(a,b,c),vec3(1.35));
    float total=weights.x+weights.y+weights.z;
    vec3 colors=(linearColor(vivid(primaryColor.rgb))*weights.x+
                 linearColor(vivid(secondaryColor.rgb))*weights.y+
                 linearColor(vivid(accentColor.rgb))*weights.z)/max(total,.0001);
    vec3 base=vivid(deepColor.rgb);
    float darkLuma=dot(base,luma);
    base*=min(1.,.09/max(darkLuma,.001));
    vec3 darkBase=base+vec3(max(0.,.04*atmosphere.w-dot(base,luma)));
    float coverage=clamp(total*tuning.x*1.75,0.,1.);
    vec3 darkField=mix(linearColor(darkBase),colors,coverage*.48);
    vec3 lightField=mix(linearColor(vec3(.965,.972,.985)),colors,coverage*.21);
    vec3 result=pow(max(mix(darkField,lightField,geometry.w),vec3(0)),vec3(1./2.2));
    vec3 neutral=mix(vec3(.025,.032,.046),vec3(.975,.979,.988),geometry.w);
    result=mix(neutral,result,atmosphere.x)*tuning.y;
    // Fixed in device-pixel space, never keyed to time: sub-LSB spatial dither
    // breaks 8-bit contours without grain or temporal shimmer.
    vec2 pixel=floor(uv*geometry.xy*max(geometry.z,1.));
    float noise=fract(52.9829189*fract(dot(pixel,vec2(.06711056,.00583715))))-.5;
    result+=noise*(atmosphere.z/255.);
    fragColor=vec4(clamp(result,0.,1.),1.)*qt_Opacity;
}
