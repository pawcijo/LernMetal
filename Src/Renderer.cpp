#include "Renderer.h"


#include <simd/simd.h>
const int Renderer::kMaxFramesInFlight = 3;

Renderer::Renderer(MTL::Device *pDevice)
    : _pDevice(pDevice->retain()), _angle(0.f), _frame(0)
{
    _pCommandQueue = _pDevice->newCommandQueue();
    buildShaders();
    buildBuffers();
    buildFrameData();

    _semaphore = dispatch_semaphore_create(Renderer::kMaxFramesInFlight);
}

Renderer::~Renderer()
{
    _pShaderLibrary->release();
    _pArgBuffer->release();
    _pVertexPositionsBuffer->release();
    _pVertexColorsBuffer->release();
    for (int i = 0; i < Renderer::kMaxFramesInFlight; ++i)
    {
        _pFrameData[i]->release();
    }
    _pPSO->release();
    _pCommandQueue->release();
    _pDevice->release();
}

void Renderer::buildShaders()
{
    using NS::StringEncoding::UTF8StringEncoding;

    const char *shaderSrc = R"(
        #include <metal_stdlib>
        using namespace metal;

        struct v2f
        {
            float4 position [[position]];
            half3 color;
        };

        struct VertexData
        {
            device float3* positions [[id(0)]];
            device float3* colors [[id(1)]];
        };

        struct FrameData
        {
            float angle;
        };

        v2f vertex vertexMain( device const VertexData* vertexData [[buffer(0)]], constant FrameData* frameData [[buffer(1)]], uint vertexId [[vertex_id]] )
        {
            float a = frameData->angle;
            float3x3 rotationMatrix = float3x3( sin(a), cos(a), 0.0, cos(a), -sin(a), 0.0, 0.0, 0.0, 1.0 );
            v2f o;
            o.position = float4( rotationMatrix * vertexData->positions[ vertexId ], 1.0 );
            o.color = half3((sin(a + vertexId*1.57)+1)/2,(cos(a+vertexId*1.57)+1)/2,(sin(2*a +vertexId*1.57)+1)/2);
            return o;
        }

        half4 fragment fragmentMain( v2f in [[stage_in]] )
        {
            return half4( in.color, 1.0 );
        }
    )";

    NS::Error *pError = nullptr;
    MTL::Library *pLibrary = _pDevice->newLibrary(NS::String::string(shaderSrc, UTF8StringEncoding), nullptr, &pError);
    if (!pLibrary)
    {
        __builtin_printf("%s", pError->localizedDescription()->utf8String());
        assert(false);
    }

    MTL::Function *pVertexFn = pLibrary->newFunction(NS::String::string("vertexMain", UTF8StringEncoding));
    MTL::Function *pFragFn = pLibrary->newFunction(NS::String::string("fragmentMain", UTF8StringEncoding));

    MTL::RenderPipelineDescriptor *pDesc = MTL::RenderPipelineDescriptor::alloc()->init();
    pDesc->setVertexFunction(pVertexFn);
    pDesc->setFragmentFunction(pFragFn);
    pDesc->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormat::PixelFormatBGRA8Unorm_sRGB);

    _pPSO = _pDevice->newRenderPipelineState(pDesc, &pError);
    if (!_pPSO)
    {
        __builtin_printf("%s", pError->localizedDescription()->utf8String());
        assert(false);
    }

    pVertexFn->release();
    pFragFn->release();
    pDesc->release();
    _pShaderLibrary = pLibrary;
}

void Renderer::buildBuffers()
{
    const size_t NumVertices = 3;

    simd::float3 positions[NumVertices] =
        {
            {-0.3f, 0.3f, 0.0f},
            {0.0f, -0.3f, 0.0f},
            {+0.3f, 0.3f, 0.0f}};

    simd::float3 colors[NumVertices] =
        {
            {1.0, 0.0f, 0.0f},
            {0.0f, 1.0, 0.0f},
            {0.0f, 0.0f, 1.0}};

    const size_t positionsDataSize = NumVertices * sizeof(simd::float3);
    const size_t colorDataSize = NumVertices * sizeof(simd::float3);

    MTL::Buffer *pVertexPositionsBuffer = _pDevice->newBuffer(positionsDataSize, MTL::ResourceStorageModeManaged);
    MTL::Buffer *pVertexColorsBuffer = _pDevice->newBuffer(colorDataSize, MTL::ResourceStorageModeManaged);

    _pVertexPositionsBuffer = pVertexPositionsBuffer;
    _pVertexColorsBuffer = pVertexColorsBuffer;

    memcpy(_pVertexPositionsBuffer->contents(), positions, positionsDataSize);
    memcpy(_pVertexColorsBuffer->contents(), colors, colorDataSize);

    _pVertexPositionsBuffer->didModifyRange(NS::Range::Make(0, _pVertexPositionsBuffer->length()));
    _pVertexColorsBuffer->didModifyRange(NS::Range::Make(0, _pVertexColorsBuffer->length()));

    using NS::StringEncoding::UTF8StringEncoding;
    assert(_pShaderLibrary);

    MTL::Function *pVertexFn = _pShaderLibrary->newFunction(NS::String::string("vertexMain", UTF8StringEncoding));
    MTL::ArgumentEncoder *pArgEncoder = pVertexFn->newArgumentEncoder(0);

    MTL::Buffer *pArgBuffer = _pDevice->newBuffer(pArgEncoder->encodedLength(), MTL::ResourceStorageModeManaged);
    _pArgBuffer = pArgBuffer;

    pArgEncoder->setArgumentBuffer(_pArgBuffer, 0);

    pArgEncoder->setBuffer(_pVertexPositionsBuffer, 0, 0);
    pArgEncoder->setBuffer(_pVertexColorsBuffer, 0, 1);

    _pArgBuffer->didModifyRange(NS::Range::Make(0, _pArgBuffer->length()));

    pVertexFn->release();
    pArgEncoder->release();
}

struct FrameData
{
    float angle;
};

void Renderer::buildFrameData()
{
    for (int i = 0; i < Renderer::kMaxFramesInFlight; ++i)
    {
        _pFrameData[i] = _pDevice->newBuffer(sizeof(FrameData), MTL::ResourceStorageModeManaged);
    }
}

void Renderer::draw(MTK::View *pView)
{
    NS::AutoreleasePool *pPool = NS::AutoreleasePool::alloc()->init();

    _frame = (_frame + 1) % Renderer::kMaxFramesInFlight;
    MTL::Buffer *pFrameDataBuffer = _pFrameData[_frame];

    MTL::CommandBuffer *pCmd = _pCommandQueue->commandBuffer();
    dispatch_semaphore_wait(_semaphore, DISPATCH_TIME_FOREVER);
    Renderer *pRenderer = this;
    pCmd->addCompletedHandler(^void(MTL::CommandBuffer *pCmd) {
      dispatch_semaphore_signal(pRenderer->_semaphore);
    });

    reinterpret_cast<FrameData *>(pFrameDataBuffer->contents())->angle = (_angle += 0.01f);
    pFrameDataBuffer->didModifyRange(NS::Range::Make(0, sizeof(FrameData)));

    MTL::RenderPassDescriptor *pRpd = pView->currentRenderPassDescriptor();
    MTL::RenderCommandEncoder *pEnc = pCmd->renderCommandEncoder(pRpd);

    pEnc->setRenderPipelineState(_pPSO);
    pEnc->setVertexBuffer(_pArgBuffer, 0, 0);
    pEnc->useResource(_pVertexPositionsBuffer, MTL::ResourceUsageRead);
    pEnc->useResource(_pVertexColorsBuffer, MTL::ResourceUsageRead);

    pEnc->setVertexBuffer(pFrameDataBuffer, 0, 1);
    pEnc->drawPrimitives(MTL::PrimitiveType::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(3));

    pEnc->endEncoding();
    pCmd->presentDrawable(pView->currentDrawable());
    pCmd->commit();

    pPool->release();
}