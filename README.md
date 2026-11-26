# RelapseTune (VST3)
Build (Windows, Visual Studio 2022 + CMake):
    cmake -B build -G "Visual Studio 17 2022"
    cmake --build build --config Release
The VST3 is copied to C:\Program Files\Common Files\VST3. In FL Studio: Plugin database -> Manage plugins -> Find installed plugins.

## No compiler? Build in the cloud
Push this folder to a GitHub repo -> Actions tab -> "Build RelapseTune VST3" -> download the RelapseTune-VST3-Windows artifact.
