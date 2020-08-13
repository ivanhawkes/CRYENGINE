#pragma once
#include <CryRenderer/Pipeline/IPipeline.h>
#include "CryCore/Containers/CryArray.h"
#include "CryExtension/ClassWeaver.h"


namespace Cry
{
	namespace Renderer
	{
		class CStageResourceProvider;

		namespace Pipeline
		{
			template<class HeapType>
			class CUsedHeap
			{
			public:
				HeapType* GetUnused()
				{
					if (m_usedCount >= m_primitives.size() || m_usedCount == 0)
						m_primitives.push_back(std::make_unique<HeapType>());

					return m_primitives[m_usedCount++].get();
				}

				void FreeUsed() { m_usedCount = 0; }

			private:
				std::vector<std::unique_ptr<HeapType>>	m_primitives;
				size_t m_usedCount;
			};

			class CPassHeap
			{
				using TList = std::forward_list<CPrimitiveRenderPass>;
				TList m_freeList;
				TList m_useList;

			public:
				CPrimitiveRenderPass* GetUsable();
				void FreeUsable();
			};

			class CPrimitiveHeap
			{
				using TList = std::forward_list<CRenderPrimitive>;
				TList m_freeList;
				TList m_useList;

			public:
				CRenderPrimitive* GetUsable();
				void FreeUsable();
			};

			class CConstantBufferHeap
			{
				using TList = std::forward_list<CConstantBufferPtr>;
				TList m_freeList;
				TList m_useList;

				CryCriticalSectionNonRecursive m_lock;

				uint32	maxBufferSize = 0;
			public:
				void SetSize(uint32	maxBufferSize);

				CConstantBuffer* GetUsable();
				void FreeUsable();
			};


			struct SStageDataStorage
			{
				CPassHeap		dynamicPasses; //TOD: Find better underlying storage type

				LocalDynArray<CPrimitiveRenderPass,2>	registeredPasses; //TOD: Find better underlying storage type

				CPrimitiveHeap		primitives; //TODO: This can probably be stored in a more clever way

				CConstantBufferHeap constantBuffers;

				std::unique_ptr<CClearRegionPass> clearRegionPass;
			};

			struct SStage : public SStageBase
			{
				SStage() {}

				SStage(const char* name, uint32 hash)
					: SStageBase(name, hash)
				{}

				SStage(string name, uint32 hash)
					: SStageBase(std::move(name), hash)
				{}

				SStageCallbacks callbacks;

				SStageDataStorage dataStorage;

				CPrimitiveRenderPass* pActivePass = nullptr;
			};

			using TStagePtr = _smart_ptr<SStage>;

			using TStageList = std::vector<TStagePtr>;
			///~Stages

			struct SRenderListEntry
			{
				SRenderListEntry(const char* name, uint32 hash, TStageRenderCallback callback, int sort = 0)
					: name(name)
					, hash(hash)
					, sort(sort)
					, renderCall(std::move(callback))
				{}

				const char* name;
				uint32		hash;
				int			sort;

				TStageRenderCallback renderCall; //TODO: Replace with small function object to store capturing render lambdas

				bool operator==(const SRenderListEntry& other) { return other.hash == hash; }
				bool operator==(const uint32& otherHash) { return otherHash == hash; }
			};

			using TRenderList = std::vector<SRenderListEntry>;

			
			class CCustomPipeline : public ICustomPipeline
			{
			public:
				CRYINTERFACE_SIMPLE(ICustomPipeline)
				CRYGENERATE_SINGLETONCLASS_GUID(CCustomPipeline, "CustomPipeline", CUSTOM_PIPELINE_GUID)

				static CCustomPipeline* Get();
				static CCustomPipeline* Create();
				static void			   Destroy();

				CCustomPipeline();

				virtual IStageResourceProvider* GetResourceProvider() const override;

				virtual void ExecuteRenderThreadCommand(TRenderThreadCommand command) override;



				virtual void CreateRenderStage(const char* name, uint32 hash, SStageCallbacks renderCallback, uint32 maxConstantBufferSize) override;

				//Render Thread
				virtual _smart_ptr<SStageBase> RT_CreateRenderStage(string name, uint32 hash, SStageCallbacks renderCallback, uint32 maxConstantBufferSize) override;
				virtual void RT_RemoveStage(uint32 hash) override;

				virtual void RT_AddRenderEntry(const char* name, uint32 hash, TStageRenderCallback renderCallback, int sort = 0) override;
				virtual void RT_RemoveRenderEntry(uint32 hash) override;

				virtual uint32 RT_AllocatePass(SStageBase& stage, const Pass::SPassCreationParams& params) override;

				virtual void RT_BeginPass(SStageBase& stage, uint32 passIDX) override;

				void RT_BeginNewPass(SStageBase& stage); //This makes no sense currently. A new pass always needs defining parameters

				virtual void RT_BeginPass(SStageBase& stage, uint32 passIDX, const Pass::SPassParams& params) override;
				virtual void RT_BeginNewPass(SStageBase& stage, const Pass::SPassParams& params) override;

				virtual void RT_AddPrimitives(SStageBase& stage, const TArray<Pass::SPrimitiveParams>& primitives) override;
				virtual void RT_AddPrimitive(SStageBase& stage, const Pass::SPrimitiveParams& primitive) override ;

				virtual void RT_EndPass(SStageBase& stageBase, bool bExecute) override;

				virtual void RT_ResetDynamicStageData(SStageBase& stage) override;


				virtual void RT_UpdatePassData(SStageBase& stageBase, uint32 passIDX, const Pass::SPassParams& params) override;

				void RT_StretchToColorTarget(ITexture* pSrc, uint32 stateMask) override final;


				void RT_ClearDepthSurface(SStageBase& stageBase, const ITexture* pDepthTex, const int nFlags, const float cDepth, const uint8 cStencil) override final;
				void RT_ClearDepthSurfaceRegion(SStageBase& stageBase, ITexture* pDepthTex, const int nFlags, const float cDepth, const uint8 cStencil, const uint numRects, const Vec4_tpl<ulong>* pRects) override final;

				void RT_ClearSurface(SStageBase& stageBase, const ITexture* pColorTex, const ColorF& cClear) override final;
				void RT_ClearSurfaceRegion(SStageBase& stageBase, ITexture* pSrc, const ColorF& cClear, const uint numRects, const Vec4_tpl<ulong>* pRects) override final;
				//

				void RT_Render();
				void Shutdown();
			protected:
				void RenderStage(SStage& stage);


			protected:
				TStageList	m_stageList;
				TRenderList m_renderList;

				std::unique_ptr<Renderer::CStageResourceProvider> m_pResourceProvider;

				std::unique_ptr<CStretchRectPass> m_copyBackPass;
				std::unique_ptr<CStretchRectPass> m_compositionPass;
			};
		}
	}
}