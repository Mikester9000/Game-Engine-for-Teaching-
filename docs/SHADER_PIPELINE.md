# Shader Pipeline — Game Engine for Teaching

This document explains how HLSL shaders move from source file to GPU execution
in this engine, and how to add, debug, and extend them.

---

## Overview

```
shaders/foo.vs.hlsl  ──► D3DCompileFromFile()  ──► ID3DBlob* bytecode  ──► ID3D11VertexShader*
shaders/foo.ps.hlsl  ──► D3DCompileFromFile()  ──► ID3DBlob* bytecode  ──► ID3D11PixelShader*
                             ▲
                    at runtime (LoadScene)
```

All shaders are compiled **at runtime** using `D3DCompileFromFile` (from `d3dcompiler.lib`).
This means:
- Shaders are loaded from disk as plain `.hlsl` text files
- No offline compilation step is needed for development
- Shader errors appear in the console immediately when the scene loads

---

## Shader Model Restrictions

All shaders in this engine target **Shader Model 4.0 (SM 4.0)**.

**Why SM 4.0?**
- Compatible with the GeForce GT 610 (DirectX Feature Level 10.0), the minimum
  hardware target for this engine.
- Supported by D3D11 WARP (the CPU software rasteriser used in CI headless mode).
- Keeps the code accessible: SM 4.0 is simpler than SM 6.x (no wave intrinsics,
  no mesh shaders, no ray tracing — just vertices, pixels, and constant buffers).

If you need SM 5.0+ features (compute shaders, unordered access views), add them
in a separate scene that gates on `D3D_FEATURE_LEVEL_11_0`.

---

## Adding a New HLSL Shader

### Step 1 — Author the shader files

Create `shaders/my_effect.vs.hlsl` and `shaders/my_effect.ps.hlsl`.
The naming convention is `<scene_name>.<stage>.hlsl`.

Minimum vertex shader template (SM 4.0):

```hlsl
// shaders/my_effect.vs.hlsl
struct VSInput  { float3 pos : POSITION; float2 uv : TEXCOORD0; };
struct VSOutput { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; };

VSOutput main(VSInput v)
{
    VSOutput o;
    o.pos = float4(v.pos, 1.0);
    o.uv  = v.uv;
    return o;
}
```

Minimum pixel shader template:

```hlsl
// shaders/my_effect.ps.hlsl
Texture2D    g_tex : register(t0);
SamplerState g_sam : register(s0);

struct PSInput { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; };

float4 main(PSInput i) : SV_TARGET
{
    return g_tex.Sample(g_sam, i.uv);
}
```

### Step 2 — Copy the shader to the output directory

In `CMakeLists.txt`, inside the `if(ENGINE_ENABLE_D3D11)` block, add your shader
to the `HLSL_SHADERS` list:

```cmake
# In CMakeLists.txt — HLSL_SHADERS list (inside if(ENGINE_ENABLE_D3D11)):
set(HLSL_SHADERS
    # ... existing shaders ...
    "${CMAKE_SOURCE_DIR}/shaders/my_effect.vs.hlsl"
    "${CMAKE_SOURCE_DIR}/shaders/my_effect.ps.hlsl"
)
```

CMake's `POST_BUILD` command copies all `HLSL_SHADERS` to the output directory
alongside `engine_sandbox.exe` so `D3DCompileFromFile` can find them by
relative path `shaders/my_effect.vs.hlsl`.

### Step 3 — Compile at runtime in LoadScene()

In `D3D11Renderer::LoadScene()` (in `D3D11Renderer.cpp`), add a new branch:

```cpp
else if (sceneName == "my_effect")
{
    // Compile vertex shader
    ComPtr<ID3DBlob> vsBlob;
    std::wstring vsPath = ToWideString(shaderDir + "my_effect.vs.hlsl");
    if (FAILED(D3DCompileFromFile(vsPath.c_str(), nullptr, nullptr,
            "main", "vs_4_0", D3DCOMPILE_ENABLE_STRICTNESS, 0,
            &vsBlob, &errorBlob)))
    {
        // TEACHING NOTE — Always check errorBlob: it contains the HLSL error
        // message (line number + description) when compilation fails.
        LOG_ERROR("my_effect VS compile failed: %s",
            errorBlob ? (char*)errorBlob->GetBufferPointer() : "no message");
        return false;
    }

    // Create the vertex shader object
    m_device->CreateVertexShader(vsBlob->GetBufferPointer(),
        vsBlob->GetBufferSize(), nullptr, &m_myScene.vs);

    // ... (compile PS, create input layout, vertex buffer, etc.)
    m_myScene.loaded = true;
}
```

### Step 4 — Draw in DrawFrame()

Add the draw call in `DrawFrame()`:

```cpp
if (m_myScene.loaded && m_currentScene == "my_effect")
{
    SCOPED_GPU_EVENT(m_context, L"My Effect");   // visible in RenderDoc / PIX
    DrawMyEffect();
}
```

### Step 5 — Register the headless scene in main.cpp

In `src/sandbox/main.cpp`, add a `--scene my_effect` acceptance test:

```cpp
else if (scene == "my_effect")
{
    // TEACHING NOTE: LoadScene returns true on success.
    if (!renderer->LoadScene("my_effect", shaderDir))
    {
        std::cout << "[FAIL] my_effect: LoadScene failed.\n";
        return 1;
    }
    renderer->RecordHeadlessFrame(0.1f, 0.1f, 0.1f);
    std::cout << "[PASS] my_effect\n";
    return 0;
}
```

---

## Shader Debugging

### RenderDoc (Recommended — Free, Open Source)

1. Download RenderDoc from https://renderdoc.org
2. Launch RenderDoc, click **Launch Application**, point it to `engine_sandbox.exe`
3. Run a scene (`--scene pbr_ibl` or interactive `--scene game`)
4. Press **F12** (or click Capture Frame) to capture a frame
5. In the **Event Browser**, navigate to the labelled render passes:
   - `Shadow Scene` → shadow depth pass + PCF lit pass
   - `PBR IBL` → GGX NDF + Smith G + Schlick F + IBL
   - `Bloom Scene` → bright-pass + blur + composite
6. Click any draw call to inspect:
   - **Mesh Viewer** — vertex positions before/after the VS
   - **Texture Viewer** — every texture bound to each shader slot
   - **Pipeline State** — vertex layout, rasteriser state, blend state

The `SCOPED_GPU_EVENT` markers in `DrawFrame()` produce the labelled sections
in the RenderDoc event browser.  They use `ID3DUserDefinedAnnotation::BeginEvent`
(D3D11's built-in annotation interface — no extra package needed).

### PIX for Windows

1. Install PIX from https://devblogs.microsoft.com/pix/download/
2. **GPU Capture** → point to `engine_sandbox.exe` → capture → inspect events
3. The same `SCOPED_GPU_EVENT` labels appear in the PIX timeline.

### Shader Printf Debugging

D3D11 does not support `printf` in shaders directly.  To debug a shader value:
1. Write it to a render target channel (e.g., output `float4(debugValue, 0, 0, 1)`)
2. Capture in RenderDoc and inspect the Texture Viewer output

---

## Future: Offline FXC Compilation (BP-3)

The current setup compiles shaders at runtime.  The planned BP-3 item will add:

1. `find_program(FXC_EXECUTABLE fxc ...)` in `CMakeLists.txt`
2. `add_custom_command` to produce `.cso` files from all HLSL sources
3. Build-time shader error detection (syntax errors fail `cmake --build`)
4. Optionally: `D3D11Renderer` loads `.cso` when present, falls back to source

This is deferred until the shader set stabilises.

---

## Future: DXC / Shader Model 6.x

For SM 6.x features (wave intrinsics, mesh shaders, bindless resources):
1. Install the DirectX Shader Compiler (`dxcompiler.dll`)
2. Replace `D3DCompileFromFile` calls with DXC's `IDxcCompiler3`
3. Target `vs_6_0` / `ps_6_0` profiles
4. Requires D3D12 or D3D11 with SM 6.x driver extension

This is beyond the current GT610 / SM 4.0 compatibility target.
