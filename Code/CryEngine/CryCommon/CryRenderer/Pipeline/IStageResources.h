#pragma once
#include "RendererDefinitions.h"

namespace Cry
{
	namespace Renderer
	{
		namespace Buffers
		{
			struct SBufferParams
			{
				SBufferParams(size_t elementCount, size_t stride, EBufferBindType type = EBufferBindType::Vertex, EBufferUsage usage = EBufferUsage::Dynamic)
					: elementCount(elementCount), stride(stride), type(type), usage(usage)
				{}

				EBufferBindType type = EBufferBindType::Vertex;
				EBufferUsage usage = EBufferUsage::Transient;
				void* pData = nullptr;
				size_t		stride = 0;
				size_t      elementCount = 0;
			};
		}

		//Parameters for render target creation
		struct SRTCreationParams
		{
			const char* rtName;
			int				width;
			int				height;
			ColorF			clearColor;
			ETEX_Type		textureType;
			uint32			flags;
			ETEX_Format		format;
			int				customID;
		};

		struct IStageResourceProvider
		{
			virtual _smart_ptr<ITexture>	CreateRenderTarget(const SRTCreationParams& renderTargetParams) const = 0;
			virtual std::unique_ptr<IDynTexture>	CreateDynamicRenderTarget(const SRTCreationParams& renderTargetParams) const = 0;

			virtual int						RegisterConstantName(const char* name) = 0;

			virtual void					UpdateTextureRegion(ITexture* pTexture, const IRenderer::SUpdateRect* rects, size_t numRects, const uint8* pSrcData, size_t rowPitch, ETEX_Format eSrcFormat) = 0;

			virtual InputLayoutHandle		RegisterLayout(const Shader::SInputElementDescription* pDescriptions, size_t count) = 0;
			virtual InputLayoutHandle		RegisterLayout(TArray<Shader::SInputElementDescription>& layoutDesc) = 0;
			virtual SamplerStateHandle		RegisterSamplers(const Renderer::Sampler::SState& sampler) = 0;

			virtual uintptr_t				CreateConstantBuffer(size_t sizeInBytes, const char* dbgName = nullptr) = 0;
			virtual void					FreeConstantBuffer(uintptr_t buffer) = 0;

			virtual uintptr_t				CreateOrUpdateBuffer(const Buffers::SBufferParams& params, uintptr_t bufferHandle = Buffers::CINVALID_BUFFER) = 0;
			virtual void					FreeBuffer(uintptr_t bufferHandle) = 0;;

			virtual void*					BufferBeginWrite(uintptr_t handle) = 0;
			virtual void					BufferEndWrite(uintptr_t handle) = 0;

			virtual ITexture* GetCurrentColorTarget() = 0;
			virtual ITexture* GetCurrentDepthTarget() = 0;
		};
	}
}