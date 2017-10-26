# Motivation

I wasn’t impressed with SpatialOS after all the hype, their [demo](https://www.youtube.com/watch?v=k7RCFoY2d-k&t=3m43s) is dull, and it doesn’t show off anything that couldn’t be done on a single machine.

So, I decided to give it a go on our platform - Hadean (full disclosure, I’ve only been an employee here for 5 weeks). In 5 weeks, I built this [demo](https://www.youtube.com/watch?v=w2fKRy5zC54) which I think is already shaping up to be more impressive. I have my own ideas, but I want to know what suggestions you have/improvements you’d like to see. If you have any questions about the demo or the platform I’ll be around for a while, comment or message me.

I decided to take a different approach to their grid, while the 2D hexagons they use may be better in a few cases, [their approach](https://docs.improbable.io/reference/12.0/workers/configuration/loadbalancer-config) seems overly generic for many use cases.

My approach uses an octree with cells stored by Morton (Z-Order) index. A leaf cell in the tree corresponds to a single worker/core. New cells will be dynamically spawned when the entities move out of the live area, and despawned when they’re empty. Cells will also be subdivided when the load in a single cell increases beyond a threshold, this will maintain an almost constant amount of computation per worker. All of this is made fairly trivial on Hadean, which handles spawning and communication between workers across machines and clouds.

# This Repo

This repo is a working SpatialOS prototype which spawns many worker processes across multiple machines, performing O(N^2) simulation on each. Spawning and communication between workers and machines is handled by the Hadean platform.

The simulation results are streamed in real-time from a cloud, maxing out our connection at 80mbps, at 128 workers across multiple machines. The code is the same for developing on a small laptop, and running at scale on many machines in the cloud.

The workers are compute bound, doing an O(N^2) simulation on 500 points each. By running many workers (across multiple machines) we can get far past this limit. And the Hadean platform makes all this spawning and communication as simple as running on a single machine.

I've not had to use docker/kubernetes/anything like that as Hadean sorts all that infrastructure management out. I send a single binary to it.

My approach uses an octree with cells stored by Morton (Z-Order) index. A leaf cell in the tree corresponds to a single worker/core. New cells will be dynamically spawned when the entitIes move out of the live area, and despawned when they’re empty. Cells will also be subdivided when the load in a single cell increases beyond a threshold, this will maintain an almost constant amount of computation per worker.

[Youtube Video - 48 and 128 workers](https://www.youtube.com/watch?v=w2fKRy5zC54)

Progress Diagram
![progress diagram](progress_diagram.svg)

Motivation
![motivation](mood_board.png)


- `src/master.c` is the master process in the cloud, that spawns the workers
- `src/worker.c` is the worker code that runs on each spawned process
- `src/client.c` is the local code which receives points via TCP `src/client-tcp.c` and displays them using OpenGL `src/client-ui.c`
- `src/simulate.c` is where the actual simulation is performed (coulomb attraction/repulsion and swarm behaviour, although swarm behaviour isn't well tested)
