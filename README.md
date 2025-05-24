# 3D Game Bridge Projects
> This project, initially started as a passion project, has spun-off from Leia Inc and is now standalone.
Thanks again to Leia Inc and former Dimenco for their support, we still strive to get SR in the hands of as many people as possible out of love for the technology.

This repository contains projects implementing a Game Bridge API or otherwise related to Game Bridge.

## Projects:

### Game Bridge Reshade
This is a ReShade addon that can add SR functionality to a game.

#### Installation instructions
Can be installed through [ReShade's installer with addon support](https://reshade.me/) directly.
You can find the manual installation instructions [here](INSTALL.md).

# Building with cmake
1. Clone project: `git clone https://github.com/JoeyAnthony/3DGameBridgeProjects.git`
2. Fetch all submodules with: `git submodule update --init --recursive`
3. Install the Simulated Reality SDK
4. Generate the project with Cmake 
5. Optional: Supply `-DLINK_GLAD=ON` and [download GLAD](https://gen.glad.sh/) to `third-party/glad` to build with our OpenGL weaving workaround for SR versions between 1.30.x and 1.33.1. 

## Related projects:

### 3DGameBridge: 
https://github.com/BramTeurlings/3DGameBridge
### 3DGameBridgeGUI: 
https://github.com/BramTeurlings/3DGameBridgeGUI


