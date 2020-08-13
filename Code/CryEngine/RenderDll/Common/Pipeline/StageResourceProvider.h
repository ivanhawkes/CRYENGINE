#pragma once
#include "CryRenderer/Pipeline/IStageResources.h"

namespace Cry
{
	namespace Renderer
	{
		class CStageResourceProvider : public IStageResourceProvider
		{
		public:
			virtual _smart_ptr<ITexture>	CreateRenderTarget(const SRTCreationParams& renderTargetParams) const override final;
			virtual std::unique_ptr<IDynTexture>	CreateDynamicRenderTarget(const SRTCreationParams& renderTargetParams) const override final;

			virtual int						RegisterConstantName(const char* name)  override final;


			void							UpdateTextureRegion(ITexture* pTexture, const IRenderer::SUpdateRect* rects, size_t numRects, const uint8* pSrcData, size_t rowPitch, ETEX_Format eSrcFormat) override final;
			virtual InputLayoutHandle		RegisterLayout(const Shader::SInputElementDescription* pDescriptions, size_t count) override final;
			virtual InputLayoutHandle		RegisterLayout(TArray<Shader::SInputElementDescription>& layoutDesc)  override final;
			virtual SamplerStateHandle		RegisterSamplers(const Renderer::Sampler::SState& sampler) override final;

			virtual uintptr_t				CreateConstantBuffer(size_t sizeInBytes, const char* dbgName = nullptr) override final;
			virtual void					FreeConstantBuffer(uintptr_t buffer) override final;

			virtual uintptr_t				CreateOrUpdateBuffer(const Buffers::SBufferParams& params, uintptr_t bufferHandle = Buffers::CINVALID_BUFFER) override final;
			virtual void					FreeBuffer(uintptr_t bufferHandle) override final;;

			virtual void*					BufferBeginWrite(uintptr_t handle) override final;
			virtual void					BufferEndWrite(uintptr_t handle) override final;

			CCryNameR GetConstantName(int idx) { return m_constantNameLookup[idx].second; }


			ITexture* GetCurrentColorTarget() override final;
			ITexture* GetCurrentDepthTarget() override final;
		protected:

			std::vector<std::pair<std::string, CCryNameR>>			m_constantNameLookup;
		};
	}
}