uniform sampler2D uTex0;
uniform sampler2D uTex1;
uniform sampler2D uTex2;
uniform sampler2D uTex3;

in lowp vec4     Color;
in highp vec3     Normal;
in highp vec2     TexCoord;

out highp vec4    oColor;

void main( void )
{
    oColor = texture( uTex0, TexCoord.st ) * Color;
}
