//
// Implementation for Yocto/RayTrace.
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

#include "yocto_raytrace.h"

#include <yocto/yocto_cli.h>
#include <yocto/yocto_geometry.h>
#include <yocto/yocto_parallel.h>
#include <yocto/yocto_sampling.h>
#include <yocto/yocto_shading.h>
#include <yocto/yocto_shape.h>

// -----------------------------------------------------------------------------
// IMPLEMENTATION FOR SCENE EVALUATION
// -----------------------------------------------------------------------------
namespace yocto
{

  // Generates a ray from a camera for yimg::image plane coordinate uv and
  // the lens coordinates luv.
  static ray3f eval_camera(const camera_data &camera, const vec2f &uv)
  {
    // YOUR CODE GOES HERE
    return camera_ray(camera.frame, camera.lens, camera.aspect, camera.film, uv);
  }

} // namespace yocto

// -----------------------------------------------------------------------------
// IMPLEMENTATION FOR PATH TRACING
// -----------------------------------------------------------------------------
namespace yocto
{
  vec4f xyzw(const vec3f &v, float w) { return vec4f{v.x, v.y, v.z, w}; }

  // Raytrace renderer.
  static vec4f shade_raytrace(const scene_data &scene, const bvh_scene &bvh,
                              const ray3f &ray, int bounce, rng_state &rng,
                              const raytrace_params &params)
  {
    const bvh_intersection &isec = intersect_bvh(bvh, scene, ray, false, true, params.line_as_cone, params.point_as_sphere);

    // No intersect ==> radiance from env
    if (!isec.hit)
      return xyzw(eval_environment(scene, ray.d), 1);

    // Retrieve instance, shape, pos, normals
    const instance_data &instance = scene.instances[isec.instance];
    const shape_data &shape = scene.shapes[instance.shape];
    vec3f position, normal;
    if ((!shape.points.empty() && params.point_as_sphere) || (!shape.lines.empty() && params.line_as_cone))
    {
      position = transform_point(instance.frame, isec.position);
      normal   = transform_direction(instance.frame, isec.normal);      
    }
    else
    {
      position = transform_point(
          instance.frame, eval_position(shape, isec.element, isec.uv));
      normal = transform_direction(
          instance.frame, eval_normal(shape, isec.element, isec.uv));
    }

    // Outgoing direction
    vec3f outgoing = -ray.d;

    // in object?
    if (dot(normal, outgoing) < 0)
      normal = -normal;

    // Dealing with hairs
    if (!shape.lines.empty())
      normal = orthonormalize(outgoing, normal);

    // Texcoords
    vec2f texcoords = eval_texcoord(shape, isec.element, isec.uv);
    texcoords = vec2f{fmod(texcoords.x, 1.f), fmod(texcoords.y, 1.f)};

    // eval_material
    const material_data &material = scene.materials[instance.material];
    vec3f emission = material.emission * xyz(eval_texture(scene, material.emission_tex, texcoords, true));
    vec4f color_tex = eval_texture(scene, material.color_tex, texcoords, true);
    vec3f color = material.color * xyz(color_tex); // * xyz(eval_color(scene, instance, isec.element, isec.uv));
    float roughness = material.roughness * xyz(eval_texture(scene, material.roughness_tex, texcoords, false)).y;
    roughness *= roughness;

    // Opacity
    float opacity = material.opacity * color_tex.w;
    if (rand1f(rng) < 1 - opacity)
      return shade_raytrace(scene, bvh, ray3f{position, ray.d}, bounce, rng, params);

    vec3f radiance = emission;
    if (bounce >= params.bounces)
      return xyzw(radiance, 1);

    // Matte materials
    if (material.type == material_type::matte)
    {
      vec3f incoming = sample_hemisphere_cos(normal, rand2f(rng));
      radiance += xyz(shade_raytrace(scene, bvh, ray3f{position, incoming}, bounce + 1, rng, params)) * color;
    }
    // Metals
    else if (material.type == material_type::reflective)
    {
      auto normalRefl = normal;
      if (roughness > 0.f)
      {
        auto exp = 2.f / (roughness * roughness);
        normalRefl = sample_hemisphere_cospower(exp, normal, rand2f(rng));
      }
      vec3f incoming = reflect(outgoing, normalRefl);
      radiance += fresnel_schlick(color, normalRefl, outgoing) * xyz(shade_raytrace(scene, bvh, ray3f{position, incoming}, bounce + 1, rng, params));
    }
    // Glossy
    else if (material.type == material_type::glossy)
    {
      vec3f normalRefl = normal;
      if (roughness > 0)
      {
        float exponent = 2.f / (roughness * roughness);
        normalRefl = sample_hemisphere_cospower(exponent, normal, rand2f(rng));
      }

      // Choose wether to reflect or absorb
      if (rand1f(rng) < fresnel_schlick(vec3f{0.04, 0.04, 0.04}, normalRefl, outgoing).x)
      {
        vec3f incoming = reflect(outgoing, normalRefl);
        radiance += xyz(shade_raytrace(scene, bvh, ray3f{position, incoming}, bounce + 1, rng, params));
      }
      else
      {
        vec3f incoming = sample_hemisphere_cos(normal, rand2f(rng));
        radiance += xyz(shade_raytrace(scene, bvh, ray3f{position, incoming}, bounce + 1, rng, params)) * color;
      }
    }
    // Transparent
    else if (material.type == material_type::transparent)
    {
      // Reflect or absorb?
      if (rand1f(rng) < fresnel_schlick(vec3f{0.04, 0.04, 0.04}, normal, outgoing).x)
      {
        // reflect
        vec3f incoming = reflect(outgoing, normal);
        radiance += xyz(shade_raytrace(scene, bvh, ray3f{position, incoming}, bounce + 1, rng, params));
      }
      else
      {
        vec3f incoming = -outgoing;
        radiance += material.color * xyz(shade_raytrace(scene, bvh, ray3f{position, incoming}, bounce + 1, rng, params));
      }
    }
    return xyzw(radiance, 1);
  }

  // Matte renderer.
  static vec4f shade_matte(const scene_data &scene, const bvh_scene &bvh,
                           const ray3f &ray, int bounce, rng_state &rng,
                           const raytrace_params &params)
  {
    // YOUR CODE GOES HERE
    return {0, 0, 0, 0};
  }

  // Eyelight for quick previewing.
  static vec4f shade_eyelight(const scene_data &scene, const bvh_scene &bvh,
                              const ray3f &ray, int bounce, rng_state &rng,
                              const raytrace_params &params)
  {
    const auto &intersection = intersect_bvh(bvh, scene, ray, false, true, params.line_as_cone, params.point_as_sphere);
    if (!intersection.hit)
      return zero4f; // No intersection

    // Retrieve instance, shape, normal and material info
    const instance_data &instance = scene.instances[intersection.instance];
    const auto &shape = scene.shapes[instance.shape];
    vec3f normal;
    if ((!shape.points.empty() && params.point_as_sphere) || (!shape.lines.empty() && params.line_as_cone)) 
      normal = transform_direction(instance.frame, intersection.normal);
    else 
        normal = transform_direction(instance.frame, eval_normal(shape, intersection.element, intersection.uv));
      

    // auto normal = transform_direction(instance.frame, eval_normal(shape, intersection.element, intersection.uv));
    auto material = scene.materials[instance.material];
    auto texcoords = eval_texcoord(shape, intersection.element, intersection.uv);
    vec4f color = xyzw(material.color, 1) * eval_texture(scene, material.color_tex, texcoords) * dot(normal, -ray.d);
    return color;
  }

  static vec4f shade_normal(const scene_data &scene, const bvh_scene &bvh,
                            const ray3f &ray, int bounce, rng_state &rng,
                            const raytrace_params &params)
  {
    const auto &intersection = intersect_bvh(bvh, scene, ray, false, true, params.line_as_cone, params.point_as_sphere);
    if (!intersection.hit)
      return zero4f; // No intersection

    // Retrieve instance, shape, normal
    const instance_data &instance = scene.instances[intersection.instance];
    const auto &shape = scene.shapes[instance.shape];
    vec3f normal;
    if ((!shape.points.empty() && params.point_as_sphere) || (!shape.lines.empty() && params.line_as_cone))
      normal = transform_direction(instance.frame, intersection.normal);
    else
      normal = transform_direction(instance.frame, eval_normal(shape, intersection.element, intersection.uv));

    const vec3f color = normal * 0.5f + 0.5f;
    return {color.x, color.y, color.z, 1.f};
  }

  static vec4f shade_texcoord(const scene_data &scene, const bvh_scene &bvh,
                              const ray3f &ray, int bounce, rng_state &rng,
                              const raytrace_params &params)
  {
    const auto &intersection = intersect_bvh(bvh, scene, ray, false, true, params.line_as_cone, params.point_as_sphere);
    if (!intersection.hit)
      return zero4f; // No intersection

    // Retrieve instance, shape and texcoords
    const instance_data &instance = scene.instances[intersection.instance];
    const auto &shape = scene.shapes[instance.shape];
    auto texcoords = eval_texcoord(
        shape, intersection.element, intersection.uv);
    return {fmod(texcoords.x, 1.f), fmod(texcoords.y, 1.f), 0.f, 1.f};
  }

  static vec4f shade_color(const scene_data &scene, const bvh_scene &bvh,
                           const ray3f &ray, int bounce, rng_state &rng,
                           const raytrace_params &params)
  {
    const auto &intersection = intersect_bvh(bvh, scene, ray, false, true, params.line_as_cone, params.point_as_sphere);
    if (!intersection.hit)
      return zero4f;
    const instance_data &instance = scene.instances[intersection.instance];
    const material_data &material = scene.materials[instance.material];
    return {material.color.x, material.color.y, material.color.z, 1.f};
  }

  // Trace a single ray from the camera using the given algorithm.
  using raytrace_shader_func = vec4f (*)(const scene_data &scene,
                                         const bvh_scene &bvh, const ray3f &ray, int bounce, rng_state &rng,
                                         const raytrace_params &params);
  static raytrace_shader_func get_shader(const raytrace_params &params)
  {
    switch (params.shader)
    {
    case raytrace_shader_type::raytrace:
      return shade_raytrace;
    case raytrace_shader_type::matte:
      return shade_matte;
    case raytrace_shader_type::eyelight:
      return shade_eyelight;
    case raytrace_shader_type::normal:
      return shade_normal;
    case raytrace_shader_type::texcoord:
      return shade_texcoord;
    case raytrace_shader_type::color:
      return shade_color;
    default:
    {
      throw std::runtime_error("sampler unknown");
      return nullptr;
    }
    }
  }

  // Build the bvh acceleration structure.
  bvh_scene make_bvh(const scene_data &scene, const raytrace_params &params)
  {
    return make_bvh(scene, false, false, params.noparallel);
  }

  // Init a sequence of random number generators.
  raytrace_state make_state(
      const scene_data &scene, const raytrace_params &params)
  {
    auto &camera = scene.cameras[params.camera];
    auto state = raytrace_state{};
    if (camera.aspect >= 1)
    {
      state.width = params.resolution;
      state.height = (int)round(params.resolution / camera.aspect);
    }
    else
    {
      state.height = params.resolution;
      state.width = (int)round(params.resolution * camera.aspect);
    }
    state.samples = 0;
    state.image.assign(state.width * state.height, {0, 0, 0, 0});
    state.hits.assign(state.width * state.height, 0);
    state.rngs.assign(state.width * state.height, {});
    auto rng_ = make_rng(1301081);
    for (auto &rng : state.rngs)
    {
      rng = make_rng(961748941ull, rand1i(rng_, 1 << 31) / 2 + 1);
    }
    return state;
  }

  // Progressively compute an image by calling trace_samples multiple times.
  void raytrace_samples(raytrace_state &state, const scene_data &scene,
                        const bvh_scene &bvh, const raytrace_params &params)
  {
    if (state.samples >= params.samples)
      return;
    // YOUR CODE GOES HERE
    const auto &shaderFunc = get_shader(params);
    const camera_data &sceneCamera = scene.cameras[params.camera];
    // If it's parallel
    if (params.noparallel)
    {
      for (int y = 0; y < state.height; ++y)
      {
        for (int x = 0; x < state.width; ++x)
        {
          int idx = y * state.width + x;
          vec2f randOffset = rand2f(state.rngs[idx]);
          float u = (static_cast<float>(x) + randOffset.x) / static_cast<float>(state.width);
          float v = (static_cast<float>(y) + randOffset.y) / static_cast<float>(state.height);
          vec2f uv = {u, v};
          ray3f ray = eval_camera(sceneCamera, uv);
          auto color = shaderFunc(
              scene, bvh, ray, 0, state.rngs[idx], params);
          state.image[idx] += color;
        }
      }
    }
    else
    {
      parallel_for(state.image.size(), [&state, &shaderFunc, &sceneCamera, &params, &scene, &bvh](int idx)
                   {
                     int x = idx % state.width;
                     int y = (idx / state.width) % state.height;
                     vec2f randOffset = rand2f(state.rngs[idx]);
                     float u = (static_cast<float>(x) + randOffset.x) / static_cast<float>(state.width);
                     float v = (static_cast<float>(y) + randOffset.y) / static_cast<float>(state.height);
                     vec2f uv = {u, v};
                     ray3f ray = eval_camera(sceneCamera, uv);
                     auto color = shaderFunc(
                         scene, bvh, ray, 0, state.rngs[idx], params);
                     state.image[idx] += color; });
    }
    state.samples += 1;
  }

  // Check image type
  static void check_image(
      const color_image &image, int width, int height, bool linear)
  {
    if (image.width != width || image.height != height)
      throw std::invalid_argument{"image should have the same size"};
    if (image.linear != linear)
      throw std::invalid_argument{
          linear ? "expected linear image" : "expected srgb image"};
  }

  // Get resulting render
  color_image get_render(const raytrace_state &state)
  {
    auto image = make_image(state.width, state.height, true);
    get_render(image, state);
    return image;
  }
  void get_render(color_image &image, const raytrace_state &state)
  {
    check_image(image, state.width, state.height, true);
    auto scale = 1.0f / (float)state.samples;
    for (auto idx = 0; idx < state.width * state.height; idx++)
    {
      image.pixels[idx] = state.image[idx] * scale;
    }
  }

} // namespace yocto
