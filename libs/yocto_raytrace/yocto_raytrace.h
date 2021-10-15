//
// # Yocto/RayTrace: Tiny raytracer tracer
//
//
// Yocto/RayTrace is a simple path tracer written on the Yocto/Scene model.
// Yocto/RayTrace is implemented in `yocto_raytrace.h` and `yocto_raytrace.cpp`.
//

//
// LICENSE:
//
// Copyright (c) 2016 -- 2021 Fabio Pellacini
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
//

#ifndef _YOCTO_RAYTRACE_H_
#define _YOCTO_RAYTRACE_H_

// -----------------------------------------------------------------------------
// INCLUDES
// -----------------------------------------------------------------------------

#include <yocto/yocto_bvh.h>
#include <yocto/yocto_image.h>
#include <yocto/yocto_math.h>
#include <yocto/yocto_sampling.h>
#include <yocto/yocto_scene.h>

#include <string>
#include <vector>

// -----------------------------------------------------------------------------
// SCENE AND RENDERING DATA
// -----------------------------------------------------------------------------
namespace yocto {

// Rendering state
struct raytrace_state {
  int               width   = 0;
  int               height  = 0;
  int               samples = 0;
  vector<vec4f>     image   = {};
  vector<int>       hits    = {};
  vector<rng_state> rngs    = {};
};

}  // namespace yocto

// -----------------------------------------------------------------------------
// HIGH LEVEL API
// -----------------------------------------------------------------------------
namespace yocto {

// Type of tracing algorithm
enum struct raytrace_shader_type {
  raytrace,  // path tracing
  matte,     // matte only rendering
  eyelight,  // eyelight rendering
  normal,    // normals
  texcoord,  // texcoords
  color,     // colors
};

// Options for trace functions
struct raytrace_params {
  int                  camera     = 0;
  int                  resolution = 720;
  raytrace_shader_type shader     = raytrace_shader_type::raytrace;
  int                  samples    = 512;
  int                  bounces    = 4;
  bool                 noparallel = false;
  int                  pratio     = 8;
  float                exposure   = 0;
  bool                 filmic     = false;
};

const auto raytrace_shader_names = vector<string>{
    "raytrace", "matte", "eyelight", "normal", "texcoord", "color"};

// Initialize state.
raytrace_state make_state(
    const scene_data& scene, const raytrace_params& params);

// Build the bvh acceleration structure.
bvh_scene make_bvh(const scene_data& scene, const raytrace_params& params);

// Progressively computes an image.
void raytrace_samples(raytrace_state& state, const scene_data& scene,
    const bvh_scene& bvh, const raytrace_params& params);

// Get resulting render
color_image get_render(const raytrace_state& state);
void        get_render(color_image& render, const raytrace_state& state);

}  // namespace yocto

#endif
