#ifndef GDEV_VULKAN_HPP
#define GDEV_VULKAN_HPP
#if BOO_HAS_VULKAN

#define SRGB_HACK 0

#include "IGraphicsDataFactory.hpp"
#include "IGraphicsCommandQueue.hpp"
#include "boo/IGraphicsContext.hpp"
#include "GLSLMacros.hpp"
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <mutex>
#include "boo/graphicsdev/VulkanDispatchTable.hpp"

namespace boo
{

struct VulkanContext
{
    struct LayerProperties
    {
        VkLayerProperties properties;
        std::vector<VkExtensionProperties> extensions;
    };

    std::vector<LayerProperties> m_instanceLayerProperties;
    std::vector<const char*> m_layerNames;
    std::vector<const char*> m_instanceExtensionNames;
    VkInstance m_instance = VK_NULL_HANDLE;
    std::vector<const char*> m_deviceExtensionNames;
    std::vector<VkPhysicalDevice> m_gpus;
    VkPhysicalDeviceProperties m_gpuProps;
    VkPhysicalDeviceMemoryProperties m_memoryProperties;
    VkDevice m_dev;
    uint32_t m_queueCount;
    uint32_t m_graphicsQueueFamilyIndex = UINT32_MAX;
    std::vector<VkQueueFamilyProperties> m_queueProps;
    VkQueue m_queue = VK_NULL_HANDLE;
    std::mutex m_queueLock;
    VkDescriptorSetLayout m_descSetLayout;
    VkPipelineLayout m_pipelinelayout;
    VkRenderPass m_pass;
    VkCommandPool m_loadPool;
    VkCommandBuffer m_loadCmdBuf;
    VkSampler m_linearSampler;

#if SRGB_HACK
    /* Dedicated objects for performing shader-based sRGB conversion */
    VkDescriptorSetLayout m_srgbDescSetLayout;
    VkDescriptorPool m_srgbDescPool;
    VkDescriptorSet m_srgbDescSet;
    VkPipelineLayout m_srgbPipelinelayout;
    VkRenderPass m_srgbPass;
    VkPipelineCache m_srgbPipelineCache;
    VkBuffer m_srgbVertBuf;
    VkDeviceMemory m_srgbVertBufMem;
    VkShaderModule m_srgbVert;
    VkShaderModule m_srgbFrag;
    VkPipeline m_srgbPipeline;
    VkPipeline m_srgbPipelinePreResize = VK_NULL_HANDLE;
    VkBuffer m_srgbRampTextureBuf;
    VkDeviceMemory m_srgbRampTextureMem;
    VkImage m_srgbRampTexture;
    VkImageView m_srgbRampTextureView;
#endif

    struct Window
    {
        struct SwapChain
        {
            VkFormat m_format = VK_FORMAT_UNDEFINED;
            VkSwapchainKHR m_swapChain = VK_NULL_HANDLE;
            struct Buffer
            {
                VkImage m_image = VK_NULL_HANDLE;
#if SRGB_HACK
                VkImageView m_view;
                VkFramebuffer m_framebuffer;
#endif
                VkImageLayout m_layout = VK_IMAGE_LAYOUT_UNDEFINED;
            };
            std::vector<Buffer> m_bufs;
            uint32_t m_backBuf = 0;
            void destroy(VkDevice dev)
            {
                m_bufs.clear();
                if (m_swapChain)
                {
#if SRGB_HACK
                    for (Buffer& buf : m_bufs)
                    {
                        vk::DestroyFramebuffer(dev, buf.m_framebuffer, nullptr);
                        vk::DestroyImageView(dev, buf.m_view, nullptr);
                    }
#endif
                    vk::DestroySwapchainKHR(dev, m_swapChain, nullptr);
                    m_swapChain = VK_NULL_HANDLE;
                }
                m_backBuf = 0;
            }
        } m_swapChains[2];
        uint32_t m_activeSwapChain = 0;
    };
    std::unordered_map<const boo::IWindow*, std::unique_ptr<Window>> m_windows;

    void initVulkan(const char* appName);
    void enumerateDevices();
    void initDevice();
    void initSwapChain(Window& windowCtx, VkSurfaceKHR surface, VkFormat format, VkColorSpaceKHR colorspace);
    void resizeSwapChain(Window& windowCtx, VkSurfaceKHR surface, VkFormat format, VkColorSpaceKHR colorspace);
};
extern VulkanContext g_VulkanContext;

class VulkanDataFactory : public IGraphicsDataFactory
{
    friend struct VulkanCommandQueue;
    IGraphicsContext* m_parent;
    VulkanContext* m_ctx;
    uint32_t m_drawSamples;
    static ThreadLocalPtr<struct VulkanData> m_deferredData;
    std::unordered_set<struct VulkanData*> m_committedData;
    std::mutex m_committedMutex;
    std::vector<int> m_texUnis;
    void destroyData(IGraphicsData*);
    void destroyAllData();
public:
    VulkanDataFactory(IGraphicsContext* parent, VulkanContext* ctx, uint32_t drawSamples);
    ~VulkanDataFactory() {destroyAllData();}

    Platform platform() const {return Platform::Vulkan;}
    const SystemChar* platformName() const {return _S("Vulkan");}

    class Context : public IGraphicsDataFactory::Context
    {
        friend class VulkanDataFactory;
        VulkanDataFactory& m_parent;
        Context(VulkanDataFactory& parent) : m_parent(parent) {}
    public:
        Platform platform() const {return Platform::Vulkan;}
        const SystemChar* platformName() const {return _S("Vulkan");}

        IGraphicsBufferS* newStaticBuffer(BufferUse use, const void* data, size_t stride, size_t count);
        IGraphicsBufferD* newDynamicBuffer(BufferUse use, size_t stride, size_t count);

        ITextureS* newStaticTexture(size_t width, size_t height, size_t mips, TextureFormat fmt,
                                    const void* data, size_t sz);
        ITextureSA* newStaticArrayTexture(size_t width, size_t height, size_t layers, TextureFormat fmt,
                                          const void* data, size_t sz);
        ITextureD* newDynamicTexture(size_t width, size_t height, TextureFormat fmt);
        ITextureR* newRenderTexture(size_t width, size_t height,
                                    bool enableShaderColorBinding, bool enableShaderDepthBinding);

        bool bindingNeedsVertexFormat() const {return false;}
        IVertexFormat* newVertexFormat(size_t elementCount, const VertexElementDescriptor* elements);

        IShaderPipeline* newShaderPipeline(const char* vertSource, const char* fragSource,
                                           std::vector<unsigned int>& vertBlobOut, std::vector<unsigned int>& fragBlobOut,
                                           std::vector<unsigned char>& pipelineBlob, IVertexFormat* vtxFmt,
                                           BlendFactor srcFac, BlendFactor dstFac, Primitive prim,
                                           bool depthTest, bool depthWrite, bool backfaceCulling);

        IShaderPipeline* newShaderPipeline(const char* vertSource, const char* fragSource, IVertexFormat* vtxFmt,
                                           BlendFactor srcFac, BlendFactor dstFac, Primitive prim,
                                           bool depthTest, bool depthWrite, bool backfaceCulling)
        {
            std::vector<unsigned int> vertBlob;
            std::vector<unsigned int> fragBlob;
            std::vector<unsigned char> pipelineBlob;
            return newShaderPipeline(vertSource, fragSource, vertBlob, fragBlob, pipelineBlob,
                                     vtxFmt, srcFac, dstFac, prim, depthTest, depthWrite, backfaceCulling);
        }

        IShaderDataBinding*
        newShaderDataBinding(IShaderPipeline* pipeline,
                             IVertexFormat* vtxFormat,
                             IGraphicsBuffer* vbo, IGraphicsBuffer* instVbo, IGraphicsBuffer* ibo,
                             size_t ubufCount, IGraphicsBuffer** ubufs, const PipelineStage* ubufStages,
                             const size_t* ubufOffs, const size_t* ubufSizes,
                             size_t texCount, ITexture** texs);
    };

    GraphicsDataToken commitTransaction(const FactoryCommitFunc&);
};

}

#endif
#endif // GDEV_VULKAN_HPP
