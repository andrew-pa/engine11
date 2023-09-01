# engine11
A(nother) modern Vulkan graphics engine, building on lessons learned from [eggv](https://github.com/andrew-pa/eggv).

## Screenshots
![Screenshot of test objects scene](https://github.com/andrew-pa/engine11/blob/main/screenshots/test-scene-with-lights.jpg)
![Screenshot of building scene in full view](https://github.com/andrew-pa/engine11/blob/main/screenshots/building-full.jpg)
![Screenshot of building scene up close](https://github.com/andrew-pa/engine11/blob/main/screenshots/building-close.jpg)
(Building model from Kitbash3D)

## Architecture
```mermaid
flowchart RL
  renderer
  frame_renderer[frame_renderer\nsets up and presents frames]
  imgui_renderer[imgui_renderer\nDear ImGui rendering pass]
  scene_renderer[scene_renderer\nManages scene data on GPU,\nrendering the scene with a rendering_algorithm]
  
  frame_renderer --o renderer
  imgui_renderer --o renderer
  scene_renderer --o renderer
  
  world[flecs::world\nECS manages renderable objects, cameras, lights, etc] --o scene_renderer
  asset_bundle[assent_bundle\nasset data stored on disk] --o scene_renderer
  rendering_algorithm --o scene_renderer
  gpu_static_scene_data[gpu_static_scene_data\nscene assets on GPU] --o scene_renderer
  subgraph rendering algorithms
    direction BT
    rendering_algorithm
    forward_rendering_algorithm -.-> rendering_algorithm
    forward_plus_rendering_algorithm -.-> rendering_algorithm
  end
```
