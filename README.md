# 3D Powder Toy
This project simulates 3D particle interaction in a GLSL compute shader.

This relies on [VoxGL](https://github.com/jfriedson/voxgl) for the voxel data structure and OpenGL interaction.

![Screenshot of waterfall](screenshots/waterfall.png?raw=true)
![Screenshot of pool of water](screenshots/pool.png?raw=true)

## Objective
When I started this project the goal was to utilize the GPU for both simulating and rendering voxel particles. This has the benefits of parallized computing and not needing to transfer large textues or storage buffers between the GPU and CPU.


## Best Settings on a GTX 1080
For a large world size and 30fps use:
- World demensions of 256^3
- simIterations to 1
- simPartialIter to 27
- max draw distance of 300

For a fast simulation and 60fps use:
- World demensions of 128^3
- simIterations to 4
- simPartialIter to 27
- max draw distance of 200


## Controls
Mouse
- Move - orient camera
- Left - place blocks

WASD - move

Space - up

Ctrl - down

Block selection (number keys)
- 1 - air (delete blocks)
- 2 - rock
- 3 - sand
- 4 - water


## Current Limitations
The parallization of the simulation decreases as the max velocity of particles increases. Right now the max velocity of particle is 1, so each shader invocation iterates over a 3x3x3 cube. This is to prevent race conditions between shader invocations. As the max velocity increases, the cube grows exponentially. Its size can be calculated using the formula (2 * max velocity + 1)**3. While iterating over the entire cube between renderings provides a better looking simulation, partial updates still look okay.

Worlds that are larger than ~480^3 blocks drops the fps to under 60 on my gtx 1080. This is mostly due to drawing from a dense voxel structure. For now, I've added a maximum render distance which fixes the performance issue, but I still want to render larger distances. Using an SVO will increase the complexity of the simulation, but will drastically improve rendering speed. Maybe a hybrid approach of generating an SVO from the dense data before rendering will be more performant. I will return to this after improving the performance of the simulation some more as that is now the limiting factor.


## Changelog
- I realized that the simulation can be parallelized by updating voxels as long as sufficient space between particles is provided in each sim iteration.

- Fixed a broken rendering optimization


## To-do
- Switch to a kinematic simulation instead of the current cellular automata-like approach for particle movement.

- Experiment with computing the simulation in an SVO and compare the performance to generating an SVO from the dense voxel structure before rendering.

- There is a bug where rendering the camera from a negative position results in a jittery image. Still looking for the fix.


## Dependencies
[VoxGL](https://github.com/jfriedson/voxgl)
