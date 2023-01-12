# SimulatedRealityInjector
This project contains all source for injecting SR into games running DirectX11 and 12 using ReShade by Crosire: https://github.com/crosire/reshade

## Projects:

### SR ReShade Addon
This is a ReShade addon that can add SR functionality to a game.

### SR ReShade API
An API with functions to detect games and inject SR on runtime.

### SR ReShade Injector
Injector application that uses the SR ReShade API to inject into games.

# Building with cmake
1. Clone project: `git clone https://github.com/JoeyAnthony/SimulatedRealityInjector.git`
2. Fetch all submodules with: `git submodule update --init --recursive`
3. Install the Simulated Reality SDK
3. Generate the project with Cmake
