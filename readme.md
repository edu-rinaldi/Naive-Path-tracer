# Yocto/Raytrace: Tiny Raytracer

In this homework, you will learn the basics of image synthesis by
implementing a simple, naive, path tracer. In particular, you will
learn how to

- setup camera and image synthesis loops,
- usa ray-intersection queries,
- write simple shaders,
- write a naive path tracer with simple sampling.

## Framework

The code uses the library [**Yocto/GL**](https://github.com/xelatihy/yocto-gl),
that is included in this project in the directory `yocto`.
We suggest to consult the [**documentation**](https://github.com/xelatihy/yocto-gl)
for the library. Also, since the library is getting improved
during the duration of the course, we suggest that you **star it and watch it**
on Github, so that you can notified as improvements are made.
In particular, we will use

- **yocto_math.h**: collection of math functions
- **yocto_sampling.h**: collection of sampling routines
- **yocto_shading.h**: collection of shading functions
- **yocto_image.{h,cpp}**: image data structure and image loading and saving
- **yocto_commonio.h**: helpers for writing command line apps
- **yocto_glview.{h,cpp}**: helpers for writing simple GUIs

In order to compile the code, you have to install
[Xcode](https://apps.apple.com/it/app/xcode/id497799835?mt=12)
on OsX, [Visual Studio 2019](https://visualstudio.microsoft.com/it/vs/) on Windows,
or a modern version of gcc or clang on Linux,
together with the tools [cmake](www.cmake.org) and [ninja](https://ninja-build.org).
The script `scripts/build.sh` will perform a simple build on OsX.
As discussed in class, we prefer to use
[Visual Studio Code](https://code.visualstudio.com), with
[C/C++](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools) and
[CMake Tools](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cmake-tools)
extensions, that we have configured to use for this course.

You will write your code in the file `yocto_raytrace.cpp` for functions that
are declared in `yocto_raytrace.h`. Your renderer is called by `yscenetrace.cpp`
for a command-line interface and `ysceneitraces.cpp` that show a simple
user interface.

This repository also contains tests that are executed from the command line
as shown in `run.sh`. The rendered images are saved in the `out/` directory.
The results should match the ones in the directory `check/`. High resolution
images are computed by `run-highres.sh`.

## Functionality (24 points)

In this homework you will implement the following features:

- **Main Rendering Loop** in function `raytrace_samples()`:
  - implement the main rendering loop considering only 1 sample (the loop over
    samples in done in the apps)
  - from the slides this is like the progressive rendering loop but with only
    one sample
  - update the accumulation buffer, number of samples and final image for each
    pixel of the `state` object
  - use `get_shader()` to get the shader from the options
  - implement both a simple loop over pixel and a parallel one, as shown in code
  - for each return value from the shader, clamp its color if above `params.clamp`
- **Color Shader** in function `shade_color()`:
  - implement a shader that check for intersection and returns the material color
  - use `intersect_bvh()` for intersection
- **Normal Shader** in function `shade_normal()`:
  - implement a shader that check for intersection and returns the normal as a
    color, with a scale and offset of 0.5 each
- **Texcoord Shader** in function `shade_texcoord()`:
  - implement a shader that check for intersection and returns the texture  
    coordinates as a color in the red-green channels; use `fmod()` to force them
    in the [0, 1] range
- **Eyelight Shader** in function `shade_eyelight()`:
  - implement a simple shader that compute diffuse lighting from the camera
    center as in the slides
- **Raytrace Shader** in function `shade_raytrace()`:
  - implement a shader that simulates illumination for a variety of materials
    structured following the steps in the lecture notes
  - get position, normal and texcoords; correct normals for lines
  - get material values by multiply material constants and textures
  - implement polished transmission, polished metals, rough metals,
    rough plastic, and matte shading in hte order described in the slides
  - you can use any function from Yocto/Shading such as `fresnel_schlick()`,
    and the appropriate sampling functions `sample_XXX()`
  - for matte, use the function `sample_hemisphere_cos()` and skip the 
    normalization by 2 pi
  - for rough metals, we will use a simpler version than the one used in the 
    slides; we will choose a microfacet normal in an accurate manner and then
    implement reflection as if the metal was polished, with that normal
    - `mnormal = sample_hemisphere_cospower(exponent, normal, rand2f(rng));`
    - `exponent   = 2 / (roughness * roughness)`
  - for plastic surfaces, we will use the same manner to compute the normal
  - and then apply a test with Fresnel that chooses whether to do reflection or
    diffuse

## Extra Credit (10 points)

- **refraction** (simple): implement refraction for polished surfaces in the
  `shade_raytrace()` function:
  - choose the refractive direction by using `refract()`,
  - `ior` is the index of refraction that you can obtain from the reflectivity (0.04)
  - remember to invert the index of refrasction upon exiting th surface
  - you can take inspiration from [Raytracing in one Weekend](https://raytracing.github.io/books/RayTracingInOneWeekend.html#dielectrics)
- **cell shading** (medio): implement a shder for non realistic rendering following
  [cell shading](https://roystan.net/articles/toon-shader.html)
- **matcap shading** (medio): implement a shader for non-realistic
  [matcap shading 1](http://viclw17.github.io/2016/05/01/MatCap-Shader-Showcase/)
  or [matcap shading 2](https://github.com/hughsk/matcap)
- **your own shader** (medio): write some shader from ideas found in 
  [ShaderToy](www.shadertoy.com)
- **other BVHs** (medio): the execution time is dominated by ray-scene
  intersection; let us test other BVHs to see whether we get any significant
  speed-up
  - we haev alraedy integrated Intel Embree, so this won't count here
  - integrate the BVh from [nanort](https://github.com/lighttransport/nanort) and
    [bvh](https://github.com/madmann91/bvh)
  - test all large scenes and report in the readme the rendering time and compare
    them to our base time
  - to integrate these BVHs, you can write new shaders that use them directly, 
    or insert the new Bvhs inside our Bvh data and redirect all calls to `intersect_bvh`
- **thick lines and points** (hard): the rendering os points and lines is now
  too approximate; in this extra credit we will compute these intersections accurately
  - render points as spheres and lines as capped cones
  - intersection algorithms can be found [here](https://iquilezles.org/www/articles/intersectors/intersectors.htm) with names Sphere and Rounded Cone
  - to integrate them in our code you have to (a) insert the new intersection 
    functions in `intersect_bvh` in `yocto_bvh`, (b) change the functions
    `eval_position` e `eval_normal` to compute the new position and normal, 
    or (c) change the data to return the intersection normal and position directly 
    from the intersection call
  - if you work on this point, please contact the teacher to get test files
- **volume rendering** (hard): implement rendering of simple volumes following
    [Ray tracing The Next Week](https://raytracing.github.io/books/RayTracingTheNextWeek.html#volumes)
- **new scenes** (very easy, few points): create additional scenes
  - create new images with your renderer from models assembled from the Web
  - you can edit new models in Blender and export then in glTF, using `yscene` 
    in Yocto/GL to convvert them and then adjust lighting by hand
  - your works well with environment maps and diffuse materials so I would focus 
    on those features - find environment maps on [PolyHaven](https://polyhaven.com)
  - as a starting point, try to assemble new scenes by placing new models and
    textures and environment maps - for example from
    [PolyHaven](https://polyhaven.com) or [Sketchfab](https://sketchfab.com)

For handing in, the content of the extra credit have to be described in a PDF 
called **readme.pdf**, that needs to be submitted together with code and images.
Just put for each extra credit, the name of the extra credit, a paragraph that
describes what you have implemented, one or more result images, and links to 
the resources used in your implementation.

You can produce a PDF in any way you want. One possibility is to write the file
as Markdown and convert it in PDF with VSCode plugins or other tool. 
In this manner, you can link directly the images that you hand in.

## Submission

To submit the homework, you need to pack a ZIP file that contains the code
you write and the images it generates, i.e. the ZIP with only the
`yocto_raytrace/` and `out/` directories.
If you are doing extra credit, include also `apps`, other images, and a
**`readme.pdf`** file that describes the list of implemented extra credit.
The file should be called `<cognome>_<nome>_<numero_di_matricola>.zip`,
i.e. `<lastname>_<firstname>_<studentid>.zip`, and you should exclude
all other directories. Send it on Google Classroom.
