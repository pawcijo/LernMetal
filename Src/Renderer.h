#include <Metal/Metal.hpp>
#include <MetalKit/MetalKit.hpp>

class Renderer
{
public:
    Renderer(MTL::Device *pDevice);
    ~Renderer();
    void buildShaders();
    void buildBuffers();
    void buildFrameData();
    void draw(MTK::View *pView);

private:
    MTL::Device *_pDevice;
    MTL::CommandQueue *_pCommandQueue;
    MTL::Library *_pShaderLibrary;
    MTL::RenderPipelineState *_pPSO;
    MTL::Buffer *_pArgBuffer;
   
    MTL::Buffer *_pVertexPositionsBuffer;
    MTL::Buffer *_pVertexColorsBuffer;
    MTL::Buffer *_pFrameData[3];
   
    float _angle;
    uint _current_range;
    int _frame;
    dispatch_semaphore_t _semaphore;
    static const int kMaxFramesInFlight;
};