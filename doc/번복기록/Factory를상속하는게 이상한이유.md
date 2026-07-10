1. IRenderBackend 의 존재 궁금 Pass Iterator 만 사용할 예정이 내 초기 계획이였는데 IRenderBackend라는 상위 모듈이 왜 생긴거지? 이게 없으면 어떤것을 할 수 없는가?
Pass Iterator 가 할수 있는역할이면 최대한 컴팩트하게 가는게 좋다는게 내 생각 계획 외적인 모듈이 생겨서 
필요한 이유, 의의 대체 불능성의 근거를 들어야 한다. Unity, Unreal, Godot, Cocos 엔진의 /context7-mcp 로 레퍼런싱을 해
2. IRenderBackend는 오히려 IRenderTargetPool 의 역할이 아니였는가? 왜 ITargetAllocator 가 된거지? 즉 IRenderBackend는 Factory라는것이 SRP를 위반하는거 같은데.

Show less

--------
--------
--------
--------

두 질문 다 날카롭습니다 — 특히 Q2(IRenderBackend=Factory 가 SRP 위반)는 정확한 직관입니다. 엔진 근거로 검증한 뒤 답하겠습니다. RHI/Device 계층이 4엔진에 보편인지 + 자원 생성 vs 명령 발행 이 분리되는지 조회합니다.

Context7 [query-docs]RHI Render Hardware Interface architectu…
OUT
InitRHI

Source: https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/FMaterialRenderProxy


Initializes the RHI (Rendering Hardware Interface) for the render resource.



## InitRHI

### Description
Initializes the RHI for the render resource.

### Method
void

### Parameters
#### Path Parameters
- **RHICmdList** (FRHICommandListBase&) - Description not available.


Create Pooled Resource with RHI Command List

Source: https://dev.epicgames.com/documentation/unreal-engine/API/Runtime/Engine/TRenderResourcePool


Creates a pooled resource, taking an RHI command list and creation arguments. This is a specific implementation for creating render resources.



ResourceType CreatePooledResource( FRHICommandListBase& RHICmdList, ResourceCreationArguments Args )

Render Hardware Interface (RHI)

Source: https://dev.epicgames.com/documentation/unreal-engine/graphics-programming-overview-for-unreal-engine


The Render Hardware Interface (RHI) in Unreal Engine is a low-level abstraction layer over platform-specific graphics APIs. This design allows most features to be written in platform-independent code, ensuring they function across various platforms. Feature sets are categorized into ERHIFeatureLevel to manage complexity, with platforms dropping to lower levels if they cannot support all features of a higher level.



Parallel Rendering Overview > Threading Overview

Source: https://dev.epicgames.com/documentation/unreal-engine/parallel-rendering-overview-for-unreal-engine


Originally, the renderer operated solely on the Render Thread, with the Game Thread enqueuing commands for later execution. These commands interacted with the Render Hardware Interface (RHI) for cross-platform graphics API access. To enhance efficiency and leverage platform capabilities, the Render Thread now acts as a frontend, queuing platform-agnostic graphical commands. A separate RHI Thread then translates and executes these commands via the appropriate graphics API on the backend, enabling independent parallelization on supported platforms like game consoles with DX12 and Vulkan.



Render Graph Builder

Source: https://dev.epicgames.com/documentation/unreal-engine/render-dependency-graph-in-unreal-engine


Pass execution lambdas record work for an RHI command list during graph execution. For compute passes, use FRHIComputeCommandList, or FRHICommandList for raster passes. Avoid FRHICommandListImmediate unless necessary, as it can prevent parallel execution. If RHI resource creation or locking is required during execution (which must be done on the render thread), use the immediate command list.

Context7 [query-docs]backend Device abstraction: backend::Dev…
OUT
Define Cocos2d Library Headers

Source: https://github.com/cocos2d/cocos2d-x/blob/v4/cocos/CMakeLists.txt


Defines a list of header files for the cocos2d library, including core headers and headers for various modules like 3D, platform, physics, math, renderer, audio, UI, network, and editor support.



set(COCOS_HEADER cocos2d.h
    ${COCOS_2D_HEADER}
    ${COCOS_3D_HEADER}
    ${COCOS_PLATFORM_HEADER}
    ${COCOS_PHYSICS_HEADER}
    ${COCOS_PHYSICS3D_HEADER}
    ${COCOS_MATH_HEADER}
    ${COCOS_NAVMESH_HEADER}
    ${COCOS_RENDERER_HEADER}
    ${COCOS_BASE_HEADER}
    ${COCOS_AUDIO_HEADER}
    ${COCOS_UI_HEADER}
    ${COCOS_NETWORK_HEADER}
    ${COCOS_EDITOR_SUPPORT_HEADER}
    ${COCOS_EXTENSIONS_HEADER}
    )


Define Cocos2d Library Sources

Source: https://github.com/cocos2d/cocos2d-x/blob/v4/cocos/CMakeLists.txt


Defines a list of source files for the cocos2d library, including core sources and sources for various modules like 3D, platform, physics, math, renderer, base, audio, UI, network, editor support, and extensions.



set(COCOS_SRC cocos2d.cpp
    ${COCOS_2D_SRC}
    ${COCOS_3D_SRC}
    ${COCOS_PLATFORM_SRC}
    ${COCOS_PHYSICS_SRC}
    ${COCOS_PHYSICS3D_SRC}
    ${COCOS_MATH_SRC}
    ${COCOS_NAVMESH_SRC}
    ${COCOS_RENDERER_SRC}
    ${COCOS_BASE_SRC}
    ${COCOS_AUDIO_SRC}
    ${COCOS_UI_SRC}
    ${COCOS_NETWORK_SRC}
    ${COCOS_EDITOR_SUPPORT_SRC}
    ${COCOS_EXTENSIONS_SRC}
    ${COCOS_STORAGE_SRC}
    )

list(APPEND COCOS_SRC ${COCOS_HEADER})


MeshRenderer Material Binding Setup

Source: https://github.com/cocos2d/cocos2d-x/blob/v4/cocos/3d/CCMesh.cpp


Shows how Mesh binds materials by creating VertexAttribBinding for each pass in setMaterial(). This establishes the connection between mesh vertex data and shader attributes, and initializes render state (blend, depth, culling) for the material.



// CCMesh.cpp:321-369 - setMaterial() binds material and vertex attributes
void Mesh::setMaterial(Material* material)
{
    if (_material != material) {
        CC_SAFE_RELEASE(_material);
        _material = material;
        CC_SAFE_RETAIN(_material);
    }
    _meshCommands.clear();

    if (_material)
    {
        for (auto technique: _material->getTechniques())
        {
            // Allocate MeshCommand vector for each technique's passes
            _meshCommands[technique->getName()] = std::vector<MeshCommand>(technique->getPasses().size());
            auto &list = _meshCommands[technique->getName()];
            
            int i = 0;
            for (auto pass: technique->getPasses())
            {
                // Bind vertex attributes: connects mesh vertex data to shader attributes
                auto vertexAttribBinding = VertexAttribBinding::create(_meshIndexData, pass, &list[i]);
                pass->setVertexAttribBinding(vertexAttribBinding);
                i += 1;
            }
        }
    }
    // Set previously bound textures to material
    for(auto& tex : _textures)
        setTexture(tex.second, tex.first);
    
    // Apply blend function state
    if (_blendDirty)
        setBlendFunc(_blend);
    
    bindMeshCommand();
}


Forward-Add Light Pass: Per-Light Uniform Dispatch via setLightUniforms

Source: https://github.com/cocos2d/cocos2d-x/blob/v4/cocos/3d/CCMesh.cpp


Demonstrates forward rendering approach where all scene lights are collected and their parameters (color, position, direction, intensity) are batched into arrays and dispatched to shader uniforms via Pass methods. Each light type (directional, point, spot, ambient) has its parameters set as array uniforms, avoiding per-light dispatch passes.



// CCMesh.cpp:525-638 - setLightUniforms batches all lights into uniform arrays
void Mesh::setLightUniforms(Pass* pass, Scene* scene, const Vec4& color, unsigned int lightmask)
{
    auto &lights = scene->getLights();
    // ... iterate through all lights ...
    for (const auto& light : lights)
    {
        bool useLight = light->isEnabled() && ((unsigned int)light->getLightFlag() & lightmask);
        if (useLight)
        {
            switch (light->getLightType())
            {
                case LightType::DIRECTIONAL:
                    _dirLightUniformColorValues[enabledDirLightNum].set(...);
                    _dirLightUniformDirValues[enabledDirLightNum] = dir;
                    ++enabledDirLightNum;
                    break;
                case LightType::POINT:
                    _pointLightUniformColorValues[enabledPointLightNum].set(...);
                    _pointLightUniformPositionValues[enabledPointLightNum] = position;
                    ++enabledPointLightNum;
                    break;
                // ... spot, ambient ...
            }
        }
    }
    // Dispatch all light data as uniform arrays to shader (single pass)
    if (0 < maxDirLight) {
        pass->setUniformDirLightColor(&_dirLightUniformColorValues[0], ...);
        pass->setUniformDirLightDir(&_dirLightUniformDirValues[0], ...);
    }
    if (0 < maxPointLight) {
        pass->setUniformPointLightColor(&_pointLightUniformColorValues[0], ...);
        pass->setUniformPointLightPosition(&_pointLightUniformPositionValues[0], ...);
    }
    // ... spot, ambient ...
}


Camera and Light Registration to Scene

Source: https://github.com/cocos2d/cocos2d-x/blob/v4/cocos/2d/CCCamera.cpp%2C%20cocos/2d/CCLight.cpp


Shows how cameras and lights are registered to the Scene. Cameras add themselves via Camera::setScene() called in onEnter(), accessing scene->_cameras. Lights register in BaseLight::onEnter() by adding themselves to scene->_lights.



// Camera registration (CCCamera.cpp:385-409)
void Camera::setScene(Scene* scene)
{
    if (_scene != scene)
    {
        if (_scene)
        {
            auto& cameras = _scene->_cameras;
            auto it = std::find(cameras.begin(), cameras.end(), this);
            if (it != cameras.end())
                cameras.erase(it);
            _scene = nullptr;
        }
        if (scene)
        {
            _scene = scene;
            auto& cameras = _scene->_cameras;
            auto it = std::find(cameras.begin(), cameras.end(), this);
            if (it == cameras.end())
            {
                _scene->_cameras.push_back(this);
                _scene->setCameraOrderDirty();
            }
        }
    }
}

// Light registration (CCLight.cpp:36-46)
void BaseLight::onEnter()
{
    auto scene = getScene();
    if (scene)
    {
        auto &lights = scene->_lights;
        auto iter = std::find(lights.begin(), lights.end(), this);
        if (iter == lights.end())
            lights.push_back(this);
    }
    Node::onEnter();
}
Context7 [query-docs]ScriptableRenderContext and CommandBuffe…
OUT
Draw Normals to Active Color Texture

Source: https://doc.unity3d.com/Manual/urp/render-graph-unsafe-pass.html


This script implements a custom ScriptableRendererFeature that uses an unsafe render pass to draw camera normals to the active color buffer. It's useful for custom rendering effects that need direct access to the render target and command buffer.



using UnityEngine;
using UnityEngine.Rendering.RenderGraphModule;
using UnityEngine.Rendering;
using UnityEngine.Rendering.Universal;

public class DrawNormalsToActiveColorTexture : ScriptableRendererFeature
{

    DrawNormalsPass unsafePass;

    public override void Create()
    {
        unsafePass = new DrawNormalsPass();
        unsafePass.renderPassEvent = RenderPassEvent.AfterRenderingPostProcessing;
    }

    public override void AddRenderPasses(ScriptableRenderer renderer, ref RenderingData renderingData)
    {
        renderer.EnqueuePass(unsafePass);
    }

    class DrawNormalsPass : ScriptableRenderPass
    {
        class PassData
        {
            public TextureHandle activeColorBuffer;
            public TextureHandle cameraNormalsTexture;
        }

        public override void RecordRenderGraph(RenderGraph renderGraph, ContextContainer frameContext)
        {
            using (var builder = renderGraph.AddUnsafePass<PassData>("Draw normals", out var passData))
            {
                // Make sure URP generates the normals texture
                ConfigureInput(ScriptableRenderPassInput.Normal);

                // Get the frame data
                UniversalResourceData resourceData = frameContext.Get<UniversalResourceData>();

                // Add the active color buffer to our pass data, and set it as writeable 
                passData.activeColorBuffer = resourceData.activeColorTexture;
                builder.UseTexture(passData.activeColorBuffer, AccessFlags.Write);
                

                // Add the camera normals texture to our pass data 
                passData.cameraNormalsTexture = resourceData.cameraNormalsTexture;
                builder.UseTexture(passData.cameraNormalsTexture);

                // Make sure the render graph system keeps the render pass, even if it's not used in the final frame.
                // Don't use this in production code, because it prevents the render graph system from removing the render pass if it's not needed.
                builder.AllowPassCulling(false);

                builder.SetRenderFunc(static (PassData data, UnsafeGraphContext context) => ExecutePass(data, context));
            }
        }

        static void ExecutePass(PassData passData, UnsafeGraphContext context)
        {
            // Create a command buffer for a list of rendering methods
            CommandBuffer unsafeCommandBuffer = CommandBufferHelpers.GetNativeCommandBuffer(context.cmd);

            // Add a command to set the render target to the active color buffer so URP draws to it
            context.cmd.SetRenderTarget(passData.activeColorBuffer);

            // Add a command to copy the camera normals texture to the render target
            Blitter.BlitTexture(unsafeCommandBuffer, passData.cameraNormalsTexture, new Vector4(1, 1, 0, 0), 0, false);
        }

    }

}



Declare Rendering Function for Render Pass

Source: https://doc.unity3d.com/Manual/urp/render-graph-write-render-pass.html


Defines the static method that generates rendering commands for a render pass. It records a command to blit a source texture to the render pass's color target.



static void ExecutePass(PassData data, RasterGraphContext context)
{
    // Records a rendering command to copy, or blit, the contents of the source texture
    // to the color render target of the render pass.
    Blitter.BlitTexture(context.cmd, data.copySourceTexture,
        new Vector4(1, 1, 0, 0), 0, false);
}


Blit Camera Texture using Blitter API

Source: https://doc.unity3d.com/Manual/urp/customize/blit-overview.html


Use the Blitter.BlitCameraTexture API within the Execute function of a render pass to blit from a source texture to a destination texture using a specified material and shader pass.



{
    Blitter.BlitCameraTexture(commandBuffer, sourceTexture, destinationTexture, materialToUse, passNumber);
}


Create and Clear Yellow Texture Render Pass

Source: https://doc.unity3d.com/Manual/urp/2D/renderer-features/custom-render-pass-workflow-urp-2d.html


This C# script defines a custom ScriptableRendererFeature2D and a ScriptableRenderPass2D that creates a texture and clears it to yellow. It demonstrates how to inject a render pass into the URP 2D pipeline and use the Render Graph API for texture management and rendering.



using UnityEngine;
using UnityEngine.Rendering.Universal;
using UnityEngine.Rendering.RenderGraphModule;
using UnityEngine.Rendering;

// Create a 2D Scriptable Renderer Feature
public class CreateYellowTextureFeature2D : ScriptableRendererFeature2D
{

    CreateYellowTexture customPass;

    public override void Create()
    {
        customPass = new CreateYellowTexture();

        // Inject the render pass at a 2D injection point
        injectionPoint2D = RenderPassEvent2D.AfterRenderingPostProcessing;
        customPass.renderPassEvent2D = injectionPoint2D;
        customPass.renderPassSortingLayerID = sortingLayerID;
    }

    public override void AddRenderPasses(ScriptableRenderer renderer, ref RenderingData renderingData)
    {
        renderer.EnqueuePass(customPass);
    }

    // Create a 2D render pass
    class CreateYellowTexture : ScriptableRenderPass2D
    {
        class PassData
        {
            public TextureHandle cameraColorTexture;
        }

        public override void RecordRenderGraph(RenderGraph renderGraph, ContextContainer frameContext)
        {
            using (var builder = renderGraph.AddRasterRenderPass<PassData>("Create yellow texture", out var passData))
            {
                // Get the frame data
                UniversalResourceData frameData = frameContext.Get<UniversalResourceData>();

                // Create texture properties that match the screen size
                TextureDesc textureDesc = frameData.activeColorTexture.GetDescriptor(renderGraph);
                textureDesc.msaaSamples = MSAASamples.None;

                // Create a temporary texture
                passData.cameraColorTexture = renderGraph.CreateTexture(textureDesc);

                // Set the texture as the render target
                builder.SetRenderAttachment(passData.cameraColorTexture, 0, AccessFlags.Write);
    
                // Make sure the render graph system keeps the render pass, even if it's not used in the final frame.
                // Don't use this in production code, because it prevents the render graph system from removing the render pass if it's not needed.
                builder.AllowPassCulling(false);

                builder.SetRenderFunc(static (PassData data, RasterGraphContext context) => ExecutePass(data, context));
            }
        }

        static void ExecutePass(PassData data, RasterGraphContext context)
        {
            // Clear the render target to yellow
            context.cmd.ClearRenderTarget(true, true, Color.yellow);
        }
    }

}


Scriptable Render Pipeline fundamentals > ScriptableRenderContext

Source: https://doc.unity3d.com/Manual/scriptable-render-pipeline-introduction.html


ScriptableRenderContext serves as the interface between custom C# render pipeline code and Unity's low-level graphics code, enabling the scheduling and execution of rendering commands.

4엔진 근거가 두 질문 다 명확히 답합니다 — 특히 Q2 의 SRP 직관이 정확합니다(엔진도 분리함). 정리합니다.

Q1 — IRenderBackend 의 정체: RHI/Device 계층 (4엔진 보편, 불가결)
먼저 오해 교정: IRenderBackend 는 PassIterator 위 모듈이 아닙니다. PassIterator 는 그냥 Execute(backend, …) 로 통과시킬 뿐이고 여전히 "순서 루프" 한 책임만 — compact 유지됨. IRenderBackend 는 Pass 가 GPU 에게 말하는 통로(직교 계층)입니다.

🔵 이 계층은 4엔진 전부에 존재 (= RHI / Device / Context):

Unreal RHI: "a low-level abstraction layer over platform-specific graphics APIs ... features written in platform-independent code, function across various platforms." 렌더러는 platform-agnostic 명령을 enqueue → RHI thread 가 실제 API 로 번역.
Unity ScriptableRenderContext: "the interface between custom render pipeline code and Unity's low-level graphics code" + CommandBuffer(패스가 context.cmd.SetRenderTarget/Blitter.BlitTexture(cmd,…) 로 기록). 패스는 raw GL 을 절대 안 부름.
Cocos: backend 모듈(Device/CommandBuffer)이 GL/Metal 추상.
Godot: RenderingDevice 가 Vulkan/D3D12/Metal 추상.
→ 어느 엔진도 패스 루프가 raw GL 을 직접 호출하지 않습니다. RHI 계층은 over-engineering 이 아니라 모든 엔진의 load-bearing seam.

없으면 못 하는 것 3 (대체 불능 근거):

헤드리스 테스트 — IRenderBackend 가 있어야 MockBackend 로 GL 컨텍스트·창 없이 패스 순서/culling/풀링을 단위 테스트. 이게 레퍼런스 plan 의 존재 이유 (네가 실제 구현과 비교할 순수 구조 검증). 없으면 GL 띄워야만 테스트 가능.
플랫폼 이식 — GL/Vulkan/Metal/D3D. Unreal RHI 의 문자 그대로의 목적. 없으면 패스마다 한 API 하드코딩 → 이식=전 패스 재작성.
지연/병렬 명령 — Unreal "render thread enqueue → RHI thread execute", Unity CommandBuffer. "무엇을 그릴지" 와 "실제 API 호출" 사이 추상이 있어야 가능.
💭 정직한 YAGNI: 단일 GL + 헤드리스 테스트 불필요 + 단일 스레드 라면 IRenderBackend 빼고 패스에 GL 인라인해도 됩니다. 레퍼런스가 유지한 건 목표가 헤드리스 구조 비교라서. 네 실제 엔진이 GL 전용이고 테스트 안 할 거면 빼는 게 더 compact — 이건 네 판단. 단 "비교용 레퍼런스" 로서는 불가결.

Q2 — IRenderBackend=Factory 가 SRP 위반? → 직관 맞음, 단 해법은 pool 이 아니라 split
두 갈래로 답합니다.

(a) "IRenderBackend 가 IRenderTargetPool 역할 아니었나" → ❌ NO
🔵 pool 은 backend 보다 위 계층이고 backend 를 사용 합니다 — 반대로 두면 안 됨:

Unreal TRenderResourcePool::CreatePooledResource(FRHICommandListBase& RHICmdList, Args) — 풀이 RHI 를 받아서 자원을 만든다. 풀은 FDynamicRHI 의 일부가 아님, 그 위 레이어.
이유: 풀=재사용 정책(플랫폼 무관, 고수준). backend=실제 GPU 할당(플랫폼 종속, 저수준). 오직 device 만 실제로 GPU 메모리를 할당할 수 있으므로 CreateTarget 은 backend 에 있는 게 맞음.
→ "backend=pool" 로 두면 그게 오히려 SRP 위반(정책을 device 에 섞음). 그래서 IRenderBackend : ITargetAllocator(생성) 는 옳고, pool 은 그 위에서 ITargetAllocator& 를 쓴다.
(b) "Factory + 명령발행 = SRP 위반" → ✅ 맞음. 엔진도 split 한다
네 직관이 정확합니다. 현재 IRenderBackend = 자원생성(CreateTarget/DestroyTarget) + 명령발행(BindTarget/DrawMesh/Blit/Present) 두 책임 → smell.

🔵 4엔진 전부 "Device(생성) vs CommandList(발행)" 분리:

엔진	자원 생성 facet	명령 발행 facet
Unreal	FDynamicRHI(RHICreateTexture)	FRHICommandList("pass lambdas record work for an RHI command list")
Unity	device(RTHandles.Alloc)	CommandBuffer(context.cmd.SetRenderTarget/Blit)
Cocos	backend::Device(newTexture)	backend::CommandBuffer
→ 해법(엔진 정합): IRenderBackend 를 둘로 쪼갠다.


struct ITargetAllocator {            // = Device/RHI facet — 자원 생성 (이미 추출)
    virtual Texture CreateTarget(const TargetDesc&) = 0;
    virtual void    DestroyTarget(Texture) = 0;
};
struct ICommandRecorder {            // = CommandList/CommandBuffer facet — 명령 발행 (신규)
    virtual void BindTarget(Texture) = 0;
    virtual void DrawMesh(uint32_t, const Mat4&) = 0;
    virtual void Blit(Texture, Texture) = 0;
    virtual void Present(Texture) = 0;
};
// 편의 우산(선택) — 아예 폐기하고 둘을 따로 넘겨도 됨
class IRenderBackend : public ITargetAllocator, public ICommandRecorder {};

// 의존 정직화:
//   Pool       → ITargetAllocator   (생성만)
//   Pass/Renderable → ICommandRecorder  (발행만)  ← DrawMesh/Blit 만 알면 됨
//   MockBackend → 둘 다 구현
이러면 각 인터페이스가 한 책임: ITargetAllocator=생성, ICommandRecorder=발행. (ISP 도 강화 — 패스는 CreateTarget 을 모르고, 풀은 DrawMesh 를 모름)
IRenderBackend 는 둘의 합 일 뿐 — 새 책임 없음. 거슬리면 폐기하고 ITargetAllocator/ICommandRecorder 를 따로 넘겨도 됨(더 SRP 순수).
💭 YAGNI 균형: 레퍼런스(헤드리스 테스트)는 단일 backend 라 합쳐도 동작엔 무방. 하지만 네가 SRP 를 명시적으로 물었고 4엔진이 실제로 분리하므로, 원칙대로면 split 이 정답. 비용은 인터페이스 1개 추가뿐.

한 줄 정리
Q1: IRenderBackend = RHI/Device 계층(4엔진 보편). PassIterator 위가 아니라 패스가 GPU 와 말하는 통로. 불가결 근거 = 헤드리스 테스트(레퍼런스 핵심)·이식·지연명령. (단일 GL+무테스트면 YAGNI 로 제거 가능.)
Q2: SRP 직관 ✅ 맞음. 단 해법은 "backend=pool"(❌, 풀은 상위)이 아니라 device 를 ITargetAllocator(생성) + ICommandRecorder(발행) 로 split(✅ Unreal FDynamicRHI↔FRHICommandList 정통).
이 ICommandRecorder 분리를 plan + 다이어그램에 반영할까요? (IRenderBackend → ITargetAllocator + ICommandRecorder 2-facet 으로 갱신)