#version 460

precision highp float;
precision highp int;

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;


uniform uint time;
//uniform uint random;

uniform uint blockType;
uniform bool placeBlock;


layout(binding = 0, std430) restrict readonly buffer BlockLocation {
    uvec3 blockLocation;
};

//layout(rgba8ui, binding = 1) restrict writeonly uniform uimage3D gridTexture;
layout(r32ui, binding = 1) uniform volatile coherent uimage3D gridTexture;

const uint dimension = 256;


// adapted from https://www.shadertoy.com/view/4djSRW (volume warning!)
float random4to1(vec4 p4) {
	p4  = fract(p4 * vec4(.1029f, .1035f, .0977f, .1097f));
    p4 += dot(p4, p4.ywxz + 31.27f);
    return fract((p4.x + p4.y + p4.z) * p4.w);
}
vec2 random4to2(vec4 p4) {
	p4  = fract(p4 * vec4(.1029f, .1035f, .0977f, .1097f));
    p4 += dot(p4, p4.ywxz + 31.27f);
    return fract((p4.xy + p4.zw) * p4.wx);
}


void main() {
	ivec3 pos = ivec3(gl_GlobalInvocationID.xyz);

	uint voxel = imageLoad(gridTexture, pos).r;

	//ivec3 newPos = curPos + ivec3(0, -1, 0);

	memoryBarrier();
	barrier();

	// sand
	if(voxel == 2) {
		// check if not on the ground
		if(pos.y > 0) {
			if(imageAtomicCompSwap(gridTexture, pos + ivec3(0, -1, 0), 0, 2) == 0)
				imageStore(gridTexture, pos, uvec4(0));
			else {
				ivec2 rand = ivec2(round(random4to2(vec4(pos, time)))) * 2 - 1;

				if(rand.x < 0) {
					if(imageAtomicCompSwap(gridTexture, pos + ivec3(-rand.y, -1, 0), 0, 2) == 0)
						imageStore(gridTexture, pos, uvec4(0));
					else if(imageAtomicCompSwap(gridTexture, pos + ivec3(rand.y, -1, 0), 0, 2) == 0)
						imageStore(gridTexture, pos, uvec4(0));
					else if(imageAtomicCompSwap(gridTexture, pos + ivec3(0, -1, -rand.y), 0, 2) == 0)
						imageStore(gridTexture, pos, uvec4(0));
					else if(imageAtomicCompSwap(gridTexture, pos + ivec3(0, -1, rand.y), 0, 2) == 0)
						imageStore(gridTexture, pos, uvec4(0));
				}
				else {
					if(imageAtomicCompSwap(gridTexture, pos + ivec3(0, -1, rand.y), 0, 2) == 0)
						imageStore(gridTexture, pos, uvec4(0));
					else if(imageAtomicCompSwap(gridTexture, pos + ivec3(0, -1, -rand.y), 0, 2) == 0)
						imageStore(gridTexture, pos, uvec4(0));
					else if(imageAtomicCompSwap(gridTexture, pos + ivec3(rand.y, -1, 0), 0, 2) == 0)
						imageStore(gridTexture, pos, uvec4(0));
					else if(imageAtomicCompSwap(gridTexture, pos + ivec3(-rand.y, -1, 0), 0, 2) == 0)
						imageStore(gridTexture, pos, uvec4(0));
				}
			}
		}
	}

	// water
	else if(voxel == 3) {
		// check if not on the ground
		if(pos.y > 0) {
			if(imageAtomicCompSwap(gridTexture, pos + ivec3(0, -1, 0), 0, 3) == 0)
				imageStore(gridTexture, pos, uvec4(0));
			else {
				bool moved = false;
				ivec2 rand = ivec2(round(random4to2(vec4(pos, time)))) * 2 - 1;

				if(rand.x < 0) {
					if(imageAtomicCompSwap(gridTexture, pos + ivec3(-rand.y, -1, 0), 0, 3) == 0) {
						imageStore(gridTexture, pos, uvec4(0));
						moved = true;
					}
					else if(imageAtomicCompSwap(gridTexture, pos + ivec3(rand.y, -1, 0), 0, 3) == 0) {
						imageStore(gridTexture, pos, uvec4(0));
						moved = true;
					}
					else if(imageAtomicCompSwap(gridTexture, pos + ivec3(0, -1, -rand.y), 0, 3) == 0) {
						imageStore(gridTexture, pos, uvec4(0));
						moved = true;
					}
					else if(imageAtomicCompSwap(gridTexture, pos + ivec3(0, -1, rand.y), 0, 3) == 0) {
						imageStore(gridTexture, pos, uvec4(0));
						moved = true;
					}
				}
				else {
					if(imageAtomicCompSwap(gridTexture, pos + ivec3(0, -1, rand.y), 0, 3) == 0) {
						imageStore(gridTexture, pos, uvec4(0));
						moved = true;
					}
					else if(imageAtomicCompSwap(gridTexture, pos + ivec3(0, -1, -rand.y), 0, 3) == 0) {
						imageStore(gridTexture, pos, uvec4(0));
						moved = true;
					}
					else if(imageAtomicCompSwap(gridTexture, pos + ivec3(rand.y, -1, 0), 0, 3) == 0) {
						imageStore(gridTexture, pos, uvec4(0));
						moved = true;
					}
					else if(imageAtomicCompSwap(gridTexture, pos + ivec3(-rand.y, -1, 0), 0, 3) == 0) {
						imageStore(gridTexture, pos, uvec4(0));
						moved = true;
					}
				}

				if(moved == false) {
					if(rand.x < 0) {
						if(imageAtomicCompSwap(gridTexture, pos + ivec3(rand.y, 0, 0), 0, 3) == 0)
							imageStore(gridTexture, pos, uvec4(0));
						else if(imageAtomicCompSwap(gridTexture, pos + ivec3(-rand.y, 0, 0), 0, 3) == 0)
							imageStore(gridTexture, pos, uvec4(0));
						else if(imageAtomicCompSwap(gridTexture, pos + ivec3(0, 0, rand.y), 0, 3) == 0)
							imageStore(gridTexture, pos, uvec4(0));
						else if(imageAtomicCompSwap(gridTexture, pos + ivec3(0, 0, -rand.y), 0, 3) == 0)
							imageStore(gridTexture, pos, uvec4(0));
					}
					else {
						if(imageAtomicCompSwap(gridTexture, pos + ivec3(0, 0, -rand.y), 0, 3) == 0)
							imageStore(gridTexture, pos, uvec4(0));
						else if(imageAtomicCompSwap(gridTexture, pos + ivec3(0, 0, rand.y), 0, 3) == 0)
							imageStore(gridTexture, pos, uvec4(0));
						else if(imageAtomicCompSwap(gridTexture, pos + ivec3(-rand.y, 0, 0), 0, 3) == 0)
							imageStore(gridTexture, pos, uvec4(0));
						else if(imageAtomicCompSwap(gridTexture, pos + ivec3(rand.y, 0, 0), 0, 3) == 0)
							imageStore(gridTexture, pos, uvec4(0));
					}
				}
			}
		}
		else {
			ivec2 rand = ivec2(round(random4to2(vec4(pos, time)))) * 2 - 1;

			if(rand.x < 0) {
				if(imageAtomicCompSwap(gridTexture, pos + ivec3(-rand.y, 0, 0), 0, 3) == 0)
					imageStore(gridTexture, pos, uvec4(0));
				else if(imageAtomicCompSwap(gridTexture, pos + ivec3(rand.y, 0, 0), 0, 3) == 0)
					imageStore(gridTexture, pos, uvec4(0));
				else if(imageAtomicCompSwap(gridTexture, pos + ivec3(0, 0, -rand.y), 0, 3) == 0)
					imageStore(gridTexture, pos, uvec4(0));
				else if(imageAtomicCompSwap(gridTexture, pos + ivec3(0, 0, rand.y), 0, 3) == 0)
					imageStore(gridTexture, pos, uvec4(0));
			}
			else {
				if(imageAtomicCompSwap(gridTexture, pos + ivec3(0, 0, rand.y), 0, 3) == 0)
					imageStore(gridTexture, pos, uvec4(0));
				else if(imageAtomicCompSwap(gridTexture, pos + ivec3(0, 0, -rand.y), 0, 3) == 0)
					imageStore(gridTexture, pos, uvec4(0));
				else if(imageAtomicCompSwap(gridTexture, pos + ivec3(rand.y, 0, 0), 0, 3) == 0)
					imageStore(gridTexture, pos, uvec4(0));
				else if(imageAtomicCompSwap(gridTexture, pos + ivec3(-rand.y, 0, 0), 0, 3) == 0)
					imageStore(gridTexture, pos, uvec4(0));
			}
		}
	}

	memoryBarrier();
	barrier();

	// place block after physics stuff
	if(placeBlock)
		if(distance(pos, blockLocation) < 5.f)
			if(imageLoad(gridTexture, pos).r == 0)
				imageStore(gridTexture, pos, uvec4(blockType));
}
