ExamplePlugin
=================

This is a tiny example extension for the FutureLooking / Celestia Nova system.

Build (Linux/macOS):

```sh
g++ -std=c++17 -fPIC -shared -o libExamplePlugin.so \
  ExamplePlugin.cpp -I../../Public -I../../Public/Core -DExamplePlugin_EXPORTS
```

Build (Windows, MSVC):

```powershell
%VCINSTALLDIR%\VC\Auxiliary\Build\vcvarsall.bat x64
cl /EHsc /LD ExamplePlugin.cpp /I ..\..\Public /I ..\..\Public\Core /DExamplePlugin_EXPORTS /Fe:ExamplePlugin.dll
```

Usage:

- Place the produced `libExamplePlugin.so` (or `ExamplePlugin.dll`) into an `Extensions/` directory.
- Use the `ModuleLoader` in the core to `LoadModule("path/to/libExamplePlugin.so")`.

The plugin exposes two C symbols (via `NOVA_DECLARE_MODULE_FACTORY`):

- `CreateModuleInstance()` — returns a new `IModuleInterface*`
- `DestroyModuleInstance(IModuleInterface*)` — destroys the instance

Canvas Menu + Resolver Example
------------------------------

This plugin now includes a clean Canvas object in [ExamplePlugin.json](../ExamplePlugin.json):

- `canvas.menuDefinitions` points to [ExamplePlugin_MenuDefinitions.json](../MenuDefinitions/ExamplePlugin_MenuDefinitions.json)
- `canvas.requirements.definitions` declares an explicit `environment.target` requirement contract
- `canvas.requirements.resolver` defines resolver strategy, descriptor access policy, and default responses

Descriptor Resolution Strategy
------------------------------

The resolver example in `ExamplePlugin.cpp` uses this order:

1. Resolve own descriptor path via `PluginRegistry::GetDescriptorPath("exampleplugin")`
2. Parse own descriptor JSON and read default responses
3. Resolve other extension descriptors via `PluginRegistry::GetDescriptorPath(extensionId)` for allowlisted IDs
4. Return normalized options (`label`, `value`, `description`)

Return Contract Strategy
------------------------

- Primary API: typed structs (`ExampleRequirementResolveRequest`, `ExampleRequirementResolveResult`)
- Optional ABI bridge: `ExamplePlugin_ResolveRequirement(const void*, void*)`

The `void*` entrypoint is only a transport bridge. The canonical contract is typed.
