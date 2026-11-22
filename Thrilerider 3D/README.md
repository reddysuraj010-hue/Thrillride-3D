# ThrillRide 3D

**FCG Mini Project — Interactive Roller Coaster Simulation Using OpenGL**

A C++/OpenGL desktop simulation demonstrating 3D transformations, Catmull–Rom curve generation, animation, perspective projection, fixed-function lighting, and keyboard/mouse interaction. The project is configured for Visual Studio on Windows and uses FreeGLUT and GLU.

## Features

- Closed, smoothly interpolated 3D roller-coaster path built from Catmull–Rom spline control points.
- Animated roller-coaster cart with seats and wheels; pause, reset, and adjustable speed.
- Three camera modes: orbit, chase, and front-seat view.
- Orbit camera controlled by dragging with the left mouse button, arrow keys, and mouse wheel.
- Track rails, cross ties, support columns, a simple park, trees, ground grid, and directional lighting.
- On-screen status and controls overlay; toggle the lights and HUD.

## Controls

| Input | Action |
| --- | --- |
| `Space` | Pause / resume the ride |
| `+` / `-` | Increase / decrease ride speed |
| `C` | Cycle orbit, chase, and front-seat cameras |
| Left mouse drag | Rotate the orbit camera |
| Mouse wheel | Zoom the orbit camera |
| Arrow keys | Adjust orbit camera angle |
| `L` | Toggle scene lighting |
| `H` | Toggle the help/status display |
| `R` | Reset ride and speed |
| `Esc` | Quit |

## Build and run (Visual Studio)

1. Install the **Desktop development with C++** workload and the Windows 10/11 SDK.
2. Install a 64-bit FreeGLUT development package that includes `include\GL\freeglut.h` and `lib\x64\freeglut.lib`. The default project location is `C:\freeglut`.
3. If FreeGLUT is elsewhere, set the `FREEGLUT_DIR` MSBuild property to its installation directory (for example, in the project's property sheet). The project links `freeglut.lib`, `opengl32.lib`, and `glu32.lib`.
4. Open `Thrilerider 3D.slnx`, choose **x64 / Debug** or **x64 / Release**, then build and run. Ensure `freeglut.dll` is beside the built executable or available on `PATH`.

The project uses the legacy OpenGL compatibility API provided by Windows OpenGL, so it does not require a separate OpenGL SDK. FreeGLUT is required for windowing and input.
