#pragma once
#include "RendererDefinitions.h"

namespace Cry
{
	namespace Renderer
	{
		namespace Pipeline
		{
			namespace Pass
			{
				enum class EDrawParamTypes : uint8
				{
					None,
					ExternalBuffers,
					RenderMesh,
					TargetClear
				};

				struct SDrawParams
				{
					EDrawParamTypes type = EDrawParamTypes::None;
					Shader::EDrawTypes		drawType = Shader::EDrawTypes::TriangleList;
				};

				struct SDrawParamsExternalBuffers : public SDrawParams
				{
					SDrawParamsExternalBuffers() { type = EDrawParamTypes::ExternalBuffers; }

					uintptr_t			inputBuffer = ~0u;
					size_t				inputSize = 0;
					uint32				inputStride = 0;
					InputLayoutHandle	inputLayout;

					uintptr_t			idxBuffer = ~0u;;
					bool				isIDX32 = sizeof(vtx_idx) == 2 ? false : true;
					size_t				idxSize = 0;

					size_t				inputOffset = 0;
					size_t				idxOffset = 0;
					uint32				inputByteOffset = 0;
				};

				struct SDrawParamsRenderMesh : public SDrawParams
				{
					SDrawParamsRenderMesh() { type = EDrawParamTypes::RenderMesh; }
					IRenderMesh* pRenderMesh = nullptr;
					size_t	vtxOffset = 0;
					size_t  idxOffset = 0;
					size_t	idxCount = 0;

				};

				struct SDrawParamsClearPass : public SDrawParams
				{
					SDrawParamsClearPass() { type = EDrawParamTypes::TargetClear; }
				};

				struct STextureParam
				{
					ITexture* pTexture = nullptr;
					uint32	  slot = 0;
				};

				struct SSamplerParam
				{
					SamplerStateHandle handle;
					uint32			   slot = 0;
				};

				struct SShaderParams
				{
					Shader::EShaderReflectFlags flags = Shader::EShaderReflectFlags::None;
					IShader* pShader = nullptr;
					uint32		techniqueLCCRC; //lowercase technique crc32
					uint64		rtMask = 0;

					//
					TArray<STextureParam>	textures;
					TArray<SSamplerParam>	samplerStates;
				};

				enum class EConstantParamsType
				{
					NoConstants,
					NamedConstants,
					ConstantBuffer,
					MultiValueConstantBuffer
				};

				struct SConstantParams
				{
					EConstantParamsType type = EConstantParamsType::NoConstants;
				};

				struct SNamedConstant
				{
					//The preregistered constant id
					uint32 constantIDX;
					Shader::EShaderClass shaderClass;
					Vec4   value;
				};

				struct SNamedConstantArray
				{
					//The preregistered constant id
					uint32 constantIDX;
					Shader::EShaderClass shaderClass;
					TArray<Vec4> values;
				};

				struct SNamedConstantsParams : public SConstantParams
				{
					SNamedConstantsParams()
					{
						type = EConstantParamsType::NamedConstants;
					}

					TArray<SNamedConstant>		 namedConstants;
					TArray<SNamedConstantArray>  namedConstantArrays;
				};

				struct SConstantBufferBase
				{
					uintptr_t	externalBuffer = Buffers::CINVALID_BUFFER;
					bool		isDirty = true;
					Shader::EConstantSlot slot = Shader::EConstantSlot::PerDraw;
					Shader::EShaderStages stages = Shader::EShaderStages::ssVertex | Shader::EShaderStages::ssPixel;
				};

				struct SBufferValuePtr
				{
					uint8* newData = nullptr;
					uint32  dataSize = 0;
				};

				struct SConstantBuffer : SConstantBufferBase
				{				
					SBufferValuePtr value;
				};

				struct SMultiVlaueConstantBuffer : SConstantBufferBase
				{
					TArray<SBufferValuePtr> values;
				};

				struct SInlineConstantParams : public SConstantParams
				{
					SInlineConstantParams() {
						type = EConstantParamsType::ConstantBuffer;
					}

					TArray<SConstantBuffer> buffers;
				};

				struct SInlineMultiValueConstantParams : public SConstantParams
				{
					SInlineMultiValueConstantParams() {
						type = EConstantParamsType::MultiValueConstantBuffer;
					}

					TArray<SMultiVlaueConstantBuffer> buffers;
				};


				struct SPrimitiveParams
				{
					SPrimitiveParams(SShaderParams& shaderParams, SConstantParams& constantParameters, SDrawParams& drawParams)
						: drawParams(drawParams)
						, shaderParams(shaderParams)
						, constantParameters(constantParameters)
					{}

					Primitives::EPrimitiveType	type = Primitives::EPrimitiveType::ePrim_Custom;

					uint32			renderStateFlags = 0;

					uint8			stencilRef = 0;
					uint8			stencilWriteMask = uint8(~0U);
					uint8			stencilReadMask = uint8(~0U);
					int				stencilState = 0;

					ECull			cullMode = eCULL_None;

					Vec4_tpl<ulong>			scissorRect = ZERO;

					SDrawParams& drawParams;
					SShaderParams& shaderParams;
					SConstantParams& constantParameters;
				};

				struct SPassParams
				{
					SRenderViewport viewPort;
					ITexture* pColorTarget = nullptr;
					ITexture* pDepthsTarget = nullptr;
					uint32			clearMask = 0;

					//Top, Left, Right, Bottom
					Vec4_tpl<ulong> scissorRect = { 0,0,0,0 };
				};


				struct SPassCreationParams : public SPassParams
				{
					string passName;
					uint32 passCrc;
				};

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
			}
		}
	}
}