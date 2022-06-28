# Voxel Particles
This project simulates particle interactions using voxels in a GLSL compute shader.

This relies on [VoxGL](https://github.com/jfriedson/voxgl) for OpenGL interaction.

The screenshots below depict a world size of 256 from a sub-performant implementation. I will update these soon.
![Screenshot of waterfall](screenshots/waterfall.png?raw=true)


## Objective
When I started this project, the goal was to utilize the GPU for both simulating and rendering voxel particles. This has the benefits of parallel computation and eliminates the need to transfer large textues or storage buffers between the GPU and CPU.


## Best Settings on a GTX 1080
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
Due to memory coherency only being guaranteed between shader invocations in the same work group and the fact that my gtx 1080 caps the group size to 8**3, the maximum velocity of a particle is 4 voxels per simulation step on my hardware.  This limitation can be cover up by comparing a random value to the simulation multiple times between renderings.

The unpredictable nature of shader execution makes it difficult to have particles interact.


## Changelog
- Some particle interaction (sand sinks in water, kind of) needs work

- Fake lighting based on direction of voxel face.

- Added grid lines. needs tweaking

- Combined the parallel and sequential approaches to provide a significant performance boost that makes simulation about 60 times faster than either of the techniques individually.

- Use an alternating flag to prevent particles from updating more than once a simulation step.

- I realized that the simulation can be parallelized by updating voxels in a checkered pattern with sufficient space between the voxels so that particles that are updated simulatenously can't move to the same empty voxel.

- Fixed a broken rendering optimization


## To-do
- Organize c++ program into functions and/or classes

- Switch to a kinematic simulation instead of the current cellular automata-like approach for particle movement.


- There is a bug where rendering the camera from a position with a negative component results in a jittery image. It seems to be a miscalculation in the ray origin or angle with negative values. Still searching for the fix.


## Dependencies
[VoxGL](https://github.com/jfriedson/voxgl)
