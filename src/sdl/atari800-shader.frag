// Fragment shader for atari800 inspired by https://github.com/Swordfish90/cool-retro-term

#version 410 core
out vec4 FragColor;
in vec2 TexCoord;
uniform sampler2D ourTexture;
uniform float scanlinesFactor;
uniform float screenCurvature;
uniform vec2 u_resolution; // Atari screen resolution
uniform float glowCoefficient;
uniform float u_pixelSpread;
uniform float u_glowCoeff;

vec2 barrel(vec2 v, vec2 resolution) {
	vec2 center = vec2(resolution.x / 2.0, resolution.y / 2.0);
	vec2 r2 = center - TexCoord;
	float distortion = dot(r2, r2) * screenCurvature;
	return v - r2 * (1.0 + distortion) * distortion;
}

float prodXY(vec2 v) {
	return v.x * v.y;
}

float maxXY(vec2 v) {
	return max(v.x, v.y);
}

float sum2(vec2 v) {
	return v.x + v.y;
}

vec2 positiveLog(vec2 x) {
	return clamp(log(x), vec2(0.0), vec2(100.0));
}

// draw CRT monitor frame with shadow and vignette
vec4 frame(vec2 texCoords, vec2 resolution) {
	if (screenCurvature == 0.0) return vec4(0.0,0.0,0.0,0.0);

	vec2 margin = vec2(0.0, 0.0);
	float frameShadowCoeff = 120.0;
	float screenShadowCoeff = 12.0;
	vec3 frameColor = vec3(0.3, 0.3, 0.35);

	vec2 coords = barrel(texCoords, resolution) * (vec2(1.0) + margin * 2.0) - margin;
	texCoords = texCoords / resolution;
	coords = coords / resolution;

	vec2 vignetteCoords = texCoords * (1.0 - texCoords.yx);
	float vignette = pow(prodXY(vignetteCoords) * 15.0, 0.25);

	vec3 color = frameColor * vec3(1.0 - vignette);
	float alpha = 0.0;

	float frameShadow = maxXY(positiveLog(-coords * frameShadowCoeff + 1.0) + positiveLog(coords * frameShadowCoeff - (frameShadowCoeff - 1.0)));
	frameShadow = max(sqrt(frameShadow), 0.0);
	color *= frameShadow;
	alpha = sum2(1.0 - step(vec2(0.0), coords) + step(vec2(1.0), coords));
	alpha = clamp(alpha, 0.0, 1.0);

	float screenShadow = 1.0 - prodXY(positiveLog(coords * screenShadowCoeff + vec2(1.0)) * positiveLog(-coords * screenShadowCoeff + vec2(screenShadowCoeff + 1.0)));
	// strength of screen shadow
	alpha = max(0.4 * screenShadow, alpha);

	return vec4(color * alpha, alpha);
}

// simulate CRT scan lines
float scanLines(vec2 screenCoords, vec2 texSize) {
	float coord = fract(screenCoords.y * texSize.y) * 2.0 - 1.0;
	return exp(-coord * coord * 1.75);
}

vec4 blur(sampler2D iChannel0, vec2 fragCoord, vec2 iResolution) {
	const float PI2 = 6.28318530718; // Pi*2

	// Gaussian blur settings
	const float directions = 16.0; // blur directions (default 16.0 - more is better but slower)
	const float quality = 4.0; // blur quality (default 4.0 - more is better but slower)
	const float size = 3.0; // blur size (radius)

	vec2 radius = size / iResolution.xy;

	// normalized pixel coordinates (from 0 to 1)
	vec2 uv = fragCoord;
	// pixel color
	vec4 color = texture(iChannel0, uv);

	// blur calculations
	for (float d = 0.0; d < PI2; d += PI2 / directions) {
		for (float i = 1.0 / quality; i <= 1.0; i += 1.0 / quality) {
			color += texture(iChannel0, uv + vec2(cos(d), sin(d)) * radius * i);		
		}
	}

	// output
	color /= quality * directions;
	return color;
}

vec3 get_tex_pixel(vec2 pos, vec2 texSize) {
	vec2 pix = 1.0 / texSize;
	vec2 p = pos - mod(pos, pix) + pix * 0.5;
	vec3 color1 = texture(ourTexture, p).rgb;
	return color1;
}

vec4 get_pixel_intensity(vec2 pos, vec2 next, vec3 left, vec3 middle, vec3 right, vec2 texSize) {
	if (u_pixelSpread <= 0.0) return vec4(middle, 1.0);

	vec2 pix = 1.0 / texSize;
	vec2 center = pos - mod(pos, pix) + pix * 0.5;
	// normalize distance
	vec2 diff = (next - center) / (pix * 0.5);
	vec2 square = diff * diff;
	square.y /= 2.0;
	const float threshold = 0.01;
	bool leftOn = dot(left, left) > threshold;
	bool rightOn = dot(right, right) > threshold;

	vec3 factor = middle;
	vec3 horz = vec3(square.x, square.x, square.x);
	vec3 vert = vec3(square.y, square.y, square.y);
	vec3 transparency;

	// simulate continuous raster line between adjacent pixels by ignoring diff.x distance
	if (leftOn && diff.x < 0.0 && diff.x > -1.0) {
		vec3 diff = abs(middle - left);
		transparency = vert + horz * diff;
	}
	else if (rightOn && diff.x > 0.0 && diff.x < 1.0) {
		vec3 diff = abs(middle - right);
		transparency = vert + horz * diff;
	}
	else {
		transparency = vert + horz;
	}

	vec3 intensity = exp(-transparency * transparency / u_pixelSpread); // pixel size
	return vec4(factor * intensity, 1.0);
}

vec4 get_pixel(vec2 pos, vec2 adjacent, vec2 texSize) {
	vec2 pix = 1.0 / texSize;
	vec2 p = pos - mod(pos, pix) + pix * 0.5;
	vec3 color0 = texture(ourTexture, p - vec2(pix.x, 0.0)).rgb;
	vec3 color1 = texture(ourTexture, p).rgb;
	vec3 color2 = texture(ourTexture, p + vec2(pix.x, 0.0)).rgb;
	return get_pixel_intensity(pos, adjacent, color0, color1, color2, texSize);
}

void main() {
	// texture size used:
	vec2 texSize = vec2(1024.0, 512.0);
	// part of the texture we actually need:
	vec2 texResolution = u_resolution / texSize;

	vec2 texCoords = barrel(TexCoord, texResolution);
	vec4 pix = get_pixel(texCoords, texCoords, texSize);
	float mask = scanLines(texCoords, texSize);
	vec4 bgnd = mix(pix, pix * mask, scanlinesFactor);

	vec4 col = pix;
	vec4 glow = blur(ourTexture, texCoords, texSize * vec2(1.0, 2.0));
	vec4 final = bgnd + glow * u_glowCoeff;

	vec4 frm = frame(TexCoord, texResolution);
	float factor = 1.0; // todo if need be: brightness <= 1.0 ? brightness : pow(brightness, 3.0);
	final = mix(final * factor, frm, frm.a);

	FragColor = final;
}
