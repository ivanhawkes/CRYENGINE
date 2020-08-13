#pragma once
#include "RenderPass.h"
#include "CryExtension/ICryUnknown.h"
#include "CryExtension/CryCreateClassInstance.h"

namespace Cry
{
	namespace Renderer
	{
		struct IStageResourceProvider;

		namespace Pipeline
		{
			
			//Base information for a custom render stage.
			struct SStageBase : _i_reference_target_t
			{
				SStageBase() {}

				SStageBase(const char* name, uint32 hash)
					: name(name)
					, hash(hash)
				{}

				SStageBase(string name, uint32 hash)
					: name(std::move(name))
					, hash(hash)
				{}

				string	name;
				uint32	hash;
				int		sortID = 0;
			};

			using TStageBasePtr = _smart_ptr<SStageBase>;


			struct StageRenderArguments
			{

			};

			struct StageCreationArguments
			{
				TStageBasePtr pStage;
			};

			struct StageDestructionsArguments
			{
				TStageBasePtr pStage;
				bool		  isShutdown;
			};

			//Called every frame to process all stage related rendering
			using TStageRenderCallback = std::function<void(StageRenderArguments& args)>;

			//Callback from the render thread once the defered stage creation is complete
			using TStageCreationCallback = std::function<void(StageCreationArguments& args)>;

			//Callback from the render thread once a stage is destroyed or the renderer is shut down
			using TStageDestructionCallback = std::function<void(StageDestructionsArguments& args)>;

			//Container for all stage related callbacks
			struct SStageCallbacks
			{
				TStageRenderCallback		renderCallback;
				TStageCreationCallback		creationCallback;
				TStageDestructionCallback	destructionCallback;
			};

			//Basic function to be executed on the render thread
			using TRenderThreadCommand = std::function<void()>;

			constexpr auto CUSTOM_PIPELINE_GUID = "{7c000a68-13f4-45a1-b917-0eafecd2d706}"_cry_guid;
			struct ICustomPipeline : public ICryUnknown
			{
				CRYINTERFACE_DECLARE_GUID(ICustomPipeline, "{d28e2d7a-775e-46cb-905f-cebb5a72af67}"_cry_guid);

				//Basic functions to provide access to renderer functionality. Best to move this somewhere else.

				//Get a point to an interface providing functions for renderer resource creation and management
				virtual IStageResourceProvider* GetResourceProvider() const = 0;

				//Execute the provided function on the render thread
				virtual void ExecuteRenderThreadCommand(TRenderThreadCommand command) = 0;

				/*
				Creates a new render stage. Once the stage is created on the render thread, the provided creation callback will be called.
				Once the stage is destructed on the render thread, for example on shutdown, the shutdown callback will be called.
				Should a render callback be provided it will be automatically invoked each frame and the per frame resources cleaned up.
				Should no render callback be provided, the user needs to manually add a render entry.
				*/
				virtual void CreateRenderStage(const char* name, uint32 hash, SStageCallbacks renderCallback, uint32 maxConstantBufferSize = 0) = 0;
		
				//Creates a new render stage on the render thread.
				virtual _smart_ptr<SStageBase> RT_CreateRenderStage(string name, uint32 hash, SStageCallbacks renderCallback, uint32 maxConstantBufferSize = 0) = 0;

				//Removes a render stage via its hash
				virtual void RT_RemoveStage(uint32 hash) = 0;

				//Adds a sortable per-frame render callback
				virtual void RT_AddRenderEntry(const char* name, uint32 hash, TStageRenderCallback renderCallback, int sort = 0) = 0;
				virtual void RT_RemoveRenderEntry(uint32 hash) = 0;

				//Allocates a new render pass for the provided rendering stage.
				//Returns the passes id
				virtual uint32 RT_AllocatePass(SStageBase& stage, const Pass::SPassCreationParams& params) = 0;

				//Begins a preallocated render pass for the provided stage. Needs to be called before primitives can be added.
				virtual void RT_BeginPass(SStageBase& stage, uint32 passIDX) = 0;

				//Begins a preallocated render pass for the provided stage. Needs to be called before primitives can be added.
				//The pass will be updated with the provided pass parameters
				virtual void RT_BeginPass(SStageBase& stage, uint32 passIDX, const Pass::SPassParams& params) = 0;
				//Begins a new temporary render pass thats allocated from a per-frame heap.
				//The pass will be updated with the provided pass parameters
				virtual void RT_BeginNewPass(SStageBase& stage, const Pass::SPassParams& params) = 0;

				//Adds a set of primitives to the currently active pass for the 
				virtual void RT_AddPrimitives(SStageBase& stage, const TArray<Pass::SPrimitiveParams>& primitives) = 0;
				virtual void RT_AddPrimitive(SStageBase& stage, const Pass::SPrimitiveParams& primitive) = 0;

				//Finish the active render pass and execute it if desired.
				virtual void RT_EndPass(SStageBase& stageBase, bool bExecute) = 0;

				virtual void RT_ResetDynamicStageData(SStageBase& stage) = 0;

				//Update the pass data for a preallocated render pass
				virtual void RT_UpdatePassData(SStageBase& stageBase, uint32 passIDX, const Pass::SPassParams& params) = 0;

				//TODO: Replace with proper api
				virtual void RT_StretchToColorTarget(ITexture* pSrc, uint32 stateMask = 0) = 0;

				virtual void RT_ClearDepthSurface(SStageBase& stageBase, const ITexture* pDepthTex, const int nFlags, const float cDepth, const uint8 cStencil) = 0;
				virtual void RT_ClearDepthSurfaceRegion(SStageBase& stageBase, ITexture* pDepthTex, const int nFlags, const float cDepth, const uint8 cStencil, const uint numRects, const Vec4_tpl<ulong>* pRects) = 0;

				virtual void RT_ClearSurface(SStageBase& stageBase, const ITexture* pColorTex, const ColorF& cClear) = 0;
				virtual void RT_ClearSurfaceRegion(SStageBase& stageBase, ITexture* pSrc, const ColorF& cClear, const uint numRects, const Vec4_tpl<ulong>* pRects) = 0;

			};
			DECLARE_SHARED_POINTERS(ICustomPipeline);

			static ICustomPipelinePtr GetOrCreateCustomPipeline()
			{
				//Create local class instance
				ICryUnknownPtr pUnknown;
				if (CryCreateClassInstance(CUSTOM_PIPELINE_GUID, pUnknown))
				{
					if (pUnknown->GetFactory()->ClassSupports(cryiidof<ICustomPipeline>()))
					{
						auto pPipeline = cryinterface_cast<ICustomPipeline>(pUnknown);
						return pPipeline;
					}
				}

				return nullptr;
			}
		}
	}
}