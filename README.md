# 3D Powder Toy
This project simulates 3D particle interaction in a GLSL compute shader. 

This relies on [VoxGL](https://github.com/jfriedson/voxgl) for the voxel data structure and OpenGL interaction.


## Limitations
The particle physics compute shader relies on atomic functionality to prevent particles from "eating" one another due to the parrallel nature of shader invocations.  As a result of this necessity, performance of voxel grids above 256 x 256 x 256 in resolution compute at a crawl on my GTX 1080 (over 2 seconds per iteration). At 256^3, I obtain ~30 simulation iterations per second.


## Dependencies
[VoxGL](https://github.com/jfriedson/voxgl)
