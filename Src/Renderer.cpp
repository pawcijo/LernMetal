#include "Renderer.h"

#include "Shader.h"

#include <simd/simd.h>
const int Renderer::kMaxFramesInFlight = 3;

Renderer::Renderer(MTL::Device *pDevice)
    : _pDevice(pDevice->retain()), _angle(0.f), _frame(0), _current_range(0)
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

    Shader shader("Shaders/Shader.metal");
    NS::Error *pError = nullptr;
    MTL::Library *pLibrary = _pDevice->newLibrary(NS::String::string(shader.ShaderString().c_str(),
                                                                     UTF8StringEncoding),
                                                  nullptr,
                                                  &pError);
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
    uint current_range;
};

void Renderer::buildFrameData()
{
    for (int i = 0; i < Renderer::kMaxFramesInFlight; ++i)
    {
        _pFrameData[i] = _pDevice->newBuffer(sizeof(FrameData), MTL::ResourceStorageModeManaged);
    }
}

bool areEqual(float a, float b, float epsilon = 0.0005) {
    return std::abs(a - b) < epsilon;
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

    // 0.01 per frame - 60 frames per second - 0.60 update per second
    _angle += 0.005;    // add color and rotation speed here
    if (_angle > 18.85) // 6pi
    {
        _angle = 0;
    }

    float pi_values[13] = {0, 1.57, 3.14,
                           4.71, 6.28, 7.85,
                           9.42, 11.00, 12.57,
                           14.14, 15.71, 17.28, 18.85};

    for (auto i = 0; i < 11; i++)
    {
        if (areEqual(pi_values[i],_angle))
        {
            _current_range += 1;

            if (_current_range > 12)
            {
                _current_range = 0;
            }
        }
    }

    reinterpret_cast<FrameData *>(pFrameDataBuffer->contents())->angle = _angle;
    reinterpret_cast<FrameData *>(pFrameDataBuffer->contents())->current_range = _current_range;

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