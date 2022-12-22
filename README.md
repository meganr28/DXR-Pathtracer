# DirectX Raytracing - Path Tracer

* Megan Reddy
* Tested on: Windows 10, AMD Ryzen 9 5900HS with Radeon Graphics @ 3301 MHz 16GB, NVIDIA GeForce RTX 3060 Laptop GPU 6GB (Personal Computer)

## Introduction

![restir](https://user-images.githubusercontent.com/20704997/209184089-3b3e94d8-379c-47cb-80b0-f9ee9a2b51eb.PNG)

This project implements the **Reservoir-based Spatiotemporal Importance Resampling (ReSTIR)** algorithm first introduced by Bitterli et al. at SIGGRAPH 2020. The goal is to render scenes with many lights in real-time, producing more converged results than standard direct lighting with a randomly chosen light. The basic idea is to sample a certain number of light candidates per pixel and store the result in a **reservoir**, discard occluded samples, combine results from previous pixels as well as adjacent pixels, and then shade the pixel with the reservoir's weight. A comparison photo of each pass is shown below. This implementation is based on Chris Wyman's tutorials on **DirectX Raytracing** and uses NVIDIA's **Falcor** framework.  

![comp4](https://user-images.githubusercontent.com/20704997/209185421-576a3238-28a9-4d73-9e10-3b44aa39a14d.PNG)

## Features

* Basic DXR Pathtracer
  * Direct illumination
  * Global illumination
  * Lambertian materials
  * Antialiasing
  * Tone mapping

* ReSTIR
  * Raytraced G-Buffer
  * Weighted RIS
  * Visibility reuse
  * Temporal reuse
  * Spatial reuse
  * A-Trous denoising

## Build Instructions

This project was developed using Chris Wyman's base code from his SIGGRAPH 2018 DirectX Raytracing tutorials.
It is dependent on NVIDIA's Falcor framework (version 3.1.0). The basic requirements can be found on the tutorial's webpage [here](http://cwyman.org/code/dxrTutors/dxr_tutors.md.html).

## Limitations and Future Work

Some limitations of this work include:

* One test scene (test on wider variety of scenes)
* Only supports point lights (no area or mesh lights)

There are many interesting directions and possibilities for future work. This includes:

* Supporting N > 1 samples (either by storing multiple reservoirs per pixel or doing separate passes of the algorithm)
* Dynamic lighting
* Extending to world-space (e.g. ReGIR)
* Global illumination (ReSTIR GI)
* Better temporal coherence

## References

[1] Chris Wyman - ["A Gentle Introduction to DirectX Raytracing"](http://cwyman.org/code/dxrTutors/dxr_tutors.md.html) (ACM SIGGRAPH 2018 Course)

[2] Bitterli et al. - [Spatiotemporal reservoir resampling for real-time ray tracing with dynamic direct lighting](https://research.nvidia.com/publication/2020-07_spatiotemporal-reservoir-resampling-real-time-ray-tracing-dynamic-direct) (ACM SIGGRAPH 2020)

[3] Shubham Sachdeva - [Spatiotemporal Reservoir Resampling (ReSTIR) - Theory and Basic Implementation](https://gamehacker1999.github.io/posts/restir/)

[4] Dammertz et al. - [Edge-Avoiding À-Trous Wavelet Transform for Fast Global Illumination Filtering](https://jo.dreggn.org/home/2010_atrous.pdf) (HPG 2010)

[5] Boksansky et al. - [Rendering Many Lights with Grid-Based Reservoirs](http://www.realtimerendering.com/raytracinggems/rtg2/) - Ray Tracing Gems II Chapter 23
