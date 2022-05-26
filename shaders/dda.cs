#version 460

precision highp float;
precision highp int;

layout(local_size_x = 16, local_size_y = 16) in;


uniform ivec2 resolution;
uniform vec3 cameraPos;
uniform mat3 cameraMat;

uniform float blockDist;
uniform bool placeBlock;

layout(binding = 0, std430) coherent buffer BlockLocation {
    uvec3 blockLocation;
};


layout(rgba8, binding = 0) restrict writeonly uniform image2D rtTexture;
layout(r32ui, binding = 1) restrict readonly uniform uimage3D gridTexture;

const int dimension = 256;


float minComponent(vec3 v) {
    return min(min(v.x, v.y), v.z);
}

float maxComponent(vec3 v) {
    return max(max(v.x, v.y), v.z);
}

vec3 castRay(vec3 origin, vec3 dir) {
    vec3 inverseDir = 1.f / dir;
    vec3 bias = inverseDir * origin;

    vec3 boundsMin = -bias;
    vec3 boundsMax = dimension * inverseDir - bias;

    float tMin = maxComponent(min(boundsMin, boundsMax));
    float tMax = minComponent(max(boundsMin, boundsMax));

	bool locationRay = all(equal(gl_GlobalInvocationID.xy, resolution/2));

	// Ray misses bounding cube
    if (tMin > tMax) {
		if(locationRay) {
			blockLocation = uvec3(dimension + uint(blockDist));
			return vec3(1.f);
		}

        return vec3(0.f);
	}

    tMin = max(tMin, 0.f);

    origin += dir * tMin;
    ivec3 pos = ivec3(origin);

    vec3 tDelta = abs(inverseDir);
    vec3 dirSign = sign(dir);
    ivec3 step = ivec3(dirSign);
    vec3 sideDist = (dirSign * (floor(origin) - origin + 0.5f) + 0.5f) * tDelta;

    float t = 0.f;

	if(locationRay) {
		if(!placeBlock) {
			ivec3 lastPos = ivec3(dimension + uint(blockDist));

			while (t < (tMax - tMin) && (tMin + t) < blockDist) {
		        bvec3 mask = lessThanEqual(sideDist.xyz, min(sideDist.yzx, sideDist.zxy));

		        float t0 = minComponent(sideDist);

				uvec4 voxel = imageLoad(gridTexture, pos);
				if(voxel.r > 0)
					break;

		        t = t0;

		        sideDist += mix(vec3(0.f), tDelta, mask);
				lastPos = pos;
		        pos += mix(ivec3(0), step, mask);
		    }

			blockLocation = uvec3(lastPos);
		}

		return vec3(1.f);
	}
	else {
	    while (t < tMax - tMin) {
	        bvec3 mask = lessThanEqual(sideDist.xyz, min(sideDist.yzx, sideDist.zxy));

	        float t0 = minComponent(sideDist);

			if(all(equal(pos, blockLocation)))
				return vec3(.5f);

			uvec4 voxel = imageLoad(gridTexture, pos);
			if(voxel.r > 0) {
				if(voxel.r == 1)
					return vec3(.26f, .25f, .22f);
				else if(voxel.r == 2)
					return vec3(.98f, .69f, .01f);
				else if(voxel.r == 3)
					return vec3(.17f, .25f, .31f);
			}

	        t = t0;

	        sideDist += mix(vec3(0.f), tDelta, mask);
	        pos += mix(ivec3(0), step, mask);
	    }

		return vec3(.1f);
	}

    return vec3(0.f);
}

void main() {
	vec3 rayVec = normalize(vec3((gl_GlobalInvocationID.xy-(0.5f*resolution))/resolution.y, 1.f)) * cameraMat;

    vec3 color = castRay(cameraPos, rayVec);

    imageStore(rtTexture, ivec2(gl_GlobalInvocationID.xy), vec4(color, 0.f));
}
