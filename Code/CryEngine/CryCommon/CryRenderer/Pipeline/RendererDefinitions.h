#pragma once

//Definitions to use the custom pipeline api.
//Most of these are renderer internal types and definitions that are exposed here to make them usable

namespace Cry
{
	namespace Renderer
	{
		
			enum class EPassClearMask : uint8
			{
				None	=	0,
				Stencil =	1u << 1,
				Color	=	1u << 2
			};
			CRY_CREATE_ENUM_FLAG_OPERATORS(EPassClearMask);
		
		namespace Sampler
		{
			enum class EReduction : uint8
			{
				Standard = 0,
				Comparison = 1,
				Minimum = 2,
				Maximum = 3,
			};

			enum class EFilter : uint8
			{
				Point = 0,
				Linear = 1,
			};

			enum class EAddressMode : uint8
			{
				Wrap = 0,
				Mirror = 1,
				Clamp = 2,
				Border = 3
			};


			struct SState
			{
				uint8 min;
				uint8 mag;
				uint8 mip;

				EAddressMode addressU;
				EAddressMode addressV;
				EAddressMode addressW;

				bool bNeverKeep = false;
			};
		}

		namespace Buffers
		{
			constexpr uint32 CINVALID_BUFFER = ~0u;

			enum class EBufferBindType : uint8
			{
				Vertex,
				Index,
				Constant,
				Count
			};


			enum class EBufferUsage : unsigned char
			{
				Immutable,					// For data that never, ever changes
				Static,                    // For long-lived data that changes infrequently (every n-frames)
				Dynamic,                   // For short-lived data that changes frequently (every frame)
				Transient,                 // For very short-lived data that can be considered garbage after first usage
				Transient_RT,              // For very short-lived data that can be considered garbage after first usage
				When_Loadingthread_Active, // yes we can ... because renderloadingthread frames not synced with mainthread frames
			};
		}

		namespace Primitives
		{
			enum class EPrimitiveType
			{
				ePrim_Triangle,
				ePrim_ProceduralTriangle,     // Triangle generated procedurally on GPU (no vertex stream)
				ePrim_ProceduralQuad,         // Quad generated procedurally on GPU (no vertex stream)
				ePrim_UnitBox,                // axis aligned box ( 0, 0, 0) - (1,1,1)
				ePrim_CenteredBox,            // axis aligned box (-1,-1,-1) - (1,1,1)
				ePrim_Projector,              // pyramid shape with sparsely tessellated ground plane
				ePrim_Projector1,             // pyramid shape with semi densely tessellated ground plane
				ePrim_Projector2,             // pyramid shape with densely tessellated ground plane
				ePrim_ClipProjector,          // same as ePrim_Projector  but with even denser tessellation
				ePrim_ClipProjector1,         // same as ePrim_Projector1 but with even denser tessellation
				ePrim_ClipProjector2,         // same as ePrim_Projector2 but with even denser tessellation
				ePrim_FullscreenQuad,         // fullscreen quad             ( 0  0, 0) - ( 1  1, 0)
				ePrim_FullscreenQuadCentered, // fullscreen quad             (-1,-1, 0) - ( 1, 1, 0). UV layout (0,0)=bottom left, (1,1)=top right
				ePrim_FullscreenQuadTess,     // tessellated fullscreen quad (-1,-1, 0) - ( 1, 1, 0). UV layout (0,0)=bottom left, (1,1)=top right
				ePrim_Custom,

				ePrim_Count,
				ePrim_First = ePrim_Triangle
			};
		}

		namespace Stencil
		{
			//Stencil flags
			enum class Flags : long
			{
				ECLEAR_ZBUFFER = 0x00000001l,
				ECLEAR_STENCIL = 0x00000002l,
				ECLEAR_RTARGET = 0x00000004l
			};

			enum class EStencilStateFunction : uint32
			{
				FSS_STENCFUNC_ALWAYS = 0x0,
				FSS_STENCFUNC_NEVER = 0x1,
				FSS_STENCFUNC_LESS = 0x2,
				FSS_STENCFUNC_LEQUAL = 0x3,
				FSS_STENCFUNC_GREATER = 0x4,
				FSS_STENCFUNC_GEQUAL = 0x5,
				FSS_STENCFUNC_EQUAL = 0x6,
				FSS_STENCFUNC_NOTEQUAL = 0x7,
				FSS_STENCFUNC_MASK = 0x7
			};

			enum class EStencilStateOp : uint32
			{
				FSS_STENCOP_KEEP = 0x0,
				FSS_STENCOP_REPLACE = 0x1,
				FSS_STENCOP_INCR = 0x2,
				FSS_STENCOP_DECR = 0x3,
				FSS_STENCOP_ZERO = 0x4,
				FSS_STENCOP_INCR_WRAP = 0x5,
				FSS_STENCOP_DECR_WRAP = 0x6,
				FSS_STENCOP_INVERT = 0x7
			};

			//Weird af stencil operations
			constexpr uint32 CFSS_STENCIL_TWOSIDED = 0x8;

			constexpr uint32 CFSS_CCW_SHIFT = 16;

			constexpr uint32 CFSS_STENCFAIL_SHIFT = 4;
			constexpr uint32 CFSS_STENCFAIL_MASK = (0x7 << CFSS_STENCFAIL_SHIFT);

			constexpr uint32 CFSS_STENCZFAIL_SHIFT = 8;
			constexpr uint32 CFSS_STENCZFAIL_MASK = (0x7 << CFSS_STENCZFAIL_SHIFT);

			constexpr uint32 CFSS_STENCPASS_SHIFT = 12;
			constexpr uint32 CFSS_STENCPASS_MASK = (0x7 << CFSS_STENCPASS_SHIFT);

			template<typename T>
			constexpr auto CSTENC_FUNC(T op) {
				return (uint32)op;
			};

			template<typename T>
			constexpr auto CSTENC_CCW_FUNC(T op) {
				return ((uint32)op << (uint32)CFSS_CCW_SHIFT);
			}

			template<typename T>
			constexpr auto CSTENCOP_FAIL(T op) {
				return((uint32)op << (uint32)CFSS_STENCFAIL_SHIFT);
			}

			template<typename T>
			constexpr auto CSTENCOP_ZFAIL(T op) {
				return((uint32)op << (uint32)CFSS_STENCZFAIL_SHIFT);
			}

			template<typename T>
			constexpr auto CSTENCOP_PASS(T op) {
				return((uint32)op << (uint32)CFSS_STENCPASS_SHIFT);
			}

			template<typename T>
			constexpr auto CSTENCOP_CCW_FAIL(T op) {
				return((uint32)op << ((uint32)CFSS_STENCFAIL_SHIFT + (uint32)CFSS_CCW_SHIFT));
			}

			template<typename T>
			constexpr auto CSTENCOP_CCW_ZFAIL(T op) {
				return((uint32)op << ((uint32)CFSS_STENCZFAIL_SHIFT + (uint32)CFSS_CCW_SHIFT));
			}

			template<typename T>
			constexpr auto CSTENCOP_CCW_PASS(T op) {
				return((uint32)op << ((uint32)CFSS_STENCPASS_SHIFT + (uint32)CFSS_CCW_SHIFT));
			}

			//Stencil masks
			constexpr uint32 CBIT_STENCIL_RESERVED = 0x80;
			constexpr uint32 CBIT_STENCIL_INSIDE_CLIPVOLUME = 0x40;
			constexpr uint32 CSTENCIL_VALUE_OUTDOORS = 0x0;

			constexpr uint32 CSTENC_VALID_BITS_NUM = 7;
			constexpr uint32 CSTENC_MAX_REF = ((1 << CSTENC_VALID_BITS_NUM) - 1);
		}

		namespace Shader
		{
			enum class EShaderClass : uint8
			{
				Vertex = 0,
				Pixel = 1,
				Geometry = 2,
				Domain = 3,
				Hull = 4,
				NumGfx = 5,

				Compute = 5,
				Num = 6
			};
			CRY_CREATE_ENUM_FLAG_OPERATORS(EShaderClass);

			enum class EShaderStages : uint8
			{
				ssVertex = BIT(0),
				ssPixel = BIT(1),
				ssGeometry = BIT(2),
				ssDomain = BIT(3),
				ssHull = BIT(4),
				ssCompute = BIT(5),

				ssCountGfx = (uint8)EShaderClass::Compute,
				ssCount = (uint8)EShaderClass::Num,
				ssNone = 0,
				ssAllWithoutCompute = ssVertex | ssPixel | ssGeometry | ssDomain | ssHull,
				ssAll = ssAllWithoutCompute | ssCompute,
			};
			CRY_CREATE_ENUM_FLAG_OPERATORS(EShaderStages);

			enum class EShaderReflectFlags : uint32
			{
				None = 0,
				ReflectVS = BIT(0),
				ReflectPS = BIT(1),
				ReflectGS = BIT(2),
				ReflectVS_PS = ReflectVS | ReflectPS,
				ReflectAll = ReflectVS_PS | ReflectGS
			};
			CRY_CREATE_ENUM_FLAG_OPERATORS(EShaderReflectFlags);

			enum class EInputElementFormat
			{
				FORMAT_R32G32B32A32_FLOAT = 2,
				R16G16B16A16_UNORM = 11,
				FORMAT_R32G32_FLOAT = 16,
				R8G8B8A8_UNORM = 28,
				R8G8B8A8_UINT = 30,
				R32_FLOAT = 41
			};

			constexpr uint32 APPEND_ALIGNED_ELEMENT = 0xffffffff;

			enum class EInputSlotClassification
			{
				PER_VERTEX_DATA = 0,
				PER_INSTANCE_DATA = 1
			};

			enum class EDrawTypes : uint16
			{
				Unknown = 9000,
				TriangleList = 4,
			};

			enum class EConstantSlot : uint8
			{
				PerDraw = 0,
				PerMaterial = 1,
				SkinQuat = 2,
				SkinQuatPrev = 3,
				PerGroup = 4,
				PerPass = 5,
				PerView = 6,
				VrProjection = 7,
			};

			struct SInputElementDescription
			{
				const char* semanticName;
				uint32		semanticIndex;
				EInputElementFormat		format; //Dxgi format
				uint32		inputSlot;
				uint32		alignedByteOffset;
				EInputSlotClassification		inputSlotClassification; //D3D11_INPUT_CLASSIFICATION
				uint32		instanceDataStepRate;
			};

			
		}
	}
}