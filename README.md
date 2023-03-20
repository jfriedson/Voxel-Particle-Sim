# Voxel Particle Simulator
This project simulates particle interactions using voxels in a GLSL compute shader.

This relies on [VoxGL](https://github.com/jfriedson/voxgl) for OpenGL interaction.

The screenshots below depict a world size of 256^3. Worlds can be scaled up to 1024^3 with a single SSBO, but the physics shader and render shader compete for computation power so the draw distance must be lowered on a GTX 1080.
![Screenshot of waterfall](screenshots/waterfall.png?raw=true)


## Objective
When I started this project, the goal was to utilize the GPU for both simulating and rendering voxel particles. This has the benefits of parallel computation and eliminates the need to transfer large textues or storage buffers between the GPU and CPU.


## Decent settings options for a GTX 1080
For a large world size and 30fps use:
- world demensions of 1024
- simIterations to 1
- max draw distance of 400

For a fast simulation and stable 60fps use:
- world demensions of 512
- simIterations to 4
- max draw distance of 350


## Controls
Mousef
- Move - orient camera
- Left - place blocks
- Scroll wheel - adjust size of block placement

WASD - move

+/- keys - adjust distance of block placement

Space - up

Ctrl - down

Block selection (number keys)
- 1 - air (delete blocks)
- 2 - rock
- 3 - sand
- 4 - water

L - toggle grid lines


## Current Limitations
Due to memory coherency only being guaranteed between shader invocations in the same work group and the fact that my gtx 1080 caps the group size to 8**3, the maximum velocity of a particle is 4 voxels per simulation step on my hardware.  This limitation is caused by the need to gurantee that voxels being processed in parallel cannot not overwrite the same voxel space. This can be covered up by comparing a random value to the fractional part of a particle's velocity.


## Changelog
- Some particle interaction is buggy, need to look into solution for two particles switching places causing an issue 

- Fake lighting based on direction of voxel face.

- Added grid lines. needs tweaking

- Combined the parallel and sequential approaches to provide a significant performance boost that makes simulation about 60 times faster than either of the techniques individually.

- Use an alternating flag to prevent particles from updating more than once a simulation step.

- I realized that the simulation can be parallelized by updating voxels in a checkered pattern with sufficient space between the voxels so that particles that are updated simulatenously can't move to the same empty voxel.

- Fixed a broken rendering optimization


## To-do
- Switch to a kinematic simulation instead of the current cellular automata-like approach for particle movement.

- There is a bug where rendering the camera from a position with a negative component results in a jittery image. It seems to be a miscalculation in the ray origin or angle with negative values. Still searching for the fix.


## Dependencies
[VoxGL](https://github.com/jfriedson/voxgl)
