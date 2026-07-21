#version 450

layout(location = 0) out vec4 out_color;
layout(location = 1) out vec2 out_motion;
layout(location = 13) in vec4 var_CurrentClip;
layout(location = 14) in vec4 var_PrevClip;

layout (constant_id = 4) const int color_mode = 0;

void main()
{
	out_motion = vec2(0.0);
	if ( any( isnan( var_PrevClip ) ) || any( isinf( var_PrevClip ) ) ||
		any( isnan( var_CurrentClip ) ) || any( isinf( var_CurrentClip ) ) ) {
		out_motion = vec2( 0.0 / 0.0 );
	} else if ( abs(var_CurrentClip.w) > 1e-6 && abs(var_PrevClip.w) > 1e-6 ) {
		vec2 currUV = var_CurrentClip.xy / var_CurrentClip.w * 0.5 + 0.5;
		vec2 prevUV = var_PrevClip.xy / var_PrevClip.w * 0.5 + 0.5;
		out_motion = currUV - prevUV;
	}

	if ( color_mode == 1 )
		out_color = vec4( 1.0, 1.0, 1.0, 1.0 ); // white
	else
	if ( color_mode == 2 )
		out_color = vec4( 0.2, 1.0, 0.2, 1.0 ); // green
	else
	if ( color_mode == 3 )
		out_color = vec4( 1.0, 0.33, 0.2, 1.0 ); // red
	else
		out_color = vec4( 0.0, 0.0, 0.0, 1.0 ); // black
}
