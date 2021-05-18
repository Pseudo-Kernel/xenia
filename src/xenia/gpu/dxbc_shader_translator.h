/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2018 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 */

#ifndef XENIA_GPU_DXBC_SHADER_TRANSLATOR_H_
#define XENIA_GPU_DXBC_SHADER_TRANSLATOR_H_

#include <cstddef>
#include <cstring>
#include <string>
#include <vector>

#include "xenia/base/math.h"
#include "xenia/base/string_buffer.h"
#include "xenia/gpu/dxbc.h"
#include "xenia/gpu/shader_translator.h"
#include "xenia/ui/graphics_provider.h"

namespace xe {
namespace gpu {

// Generates shader model 5_1 byte code (for Direct3D 12).
//
// IMPORTANT CONTRIBUTION NOTES:
//
// While DXBC may look like a flexible and high-level representation with highly
// generalized building blocks, actually it has a lot of restrictions on operand
// usage!
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// !!!DO NOT ADD ANYTHING FXC THAT WOULD NOT PRODUCE!!!
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// Before adding any sequence that you haven't seen in Xenia, try writing
// equivalent code in HLSL and running it through FXC, try with /Od, try with
// full optimization, but if you see that FXC follows a different pattern than
// what you are expecting, do what FXC does!!!
// SEE THE NOTES DXBC.H BEFORE WRITING ANYTHING RELATED TO DXBC!
class DxbcShaderTranslator : public ShaderTranslator {
 public:
  DxbcShaderTranslator(ui::GraphicsProvider::GpuVendorID vendor_id,
                       bool bindless_resources_used, bool edram_rov_used,
                       bool gamma_render_target_as_srgb = false,
                       bool msaa_2x_supported = true,
                       uint32_t draw_resolution_scale = 1,
                       bool force_emit_source_map = false);
  ~DxbcShaderTranslator() override;

  union Modification {
    // If anything in this is structure is changed in a way not compatible with
    // the previous layout, invalidate the pipeline storages by increasing this
    // version number (0xYYYYMMDD)!
    static constexpr uint32_t kVersion = 0x20210425;

    enum class DepthStencilMode : uint32_t {
      kNoModifiers,
      // [earlydepthstencil] - enable if alpha test and alpha to coverage are
      // disabled; ignored if anything in the shader blocks early Z writing.
      kEarlyHint,
      // Converting the depth to the closest 32-bit float representable exactly
      // as a 20e4 float, to support invariance in cases when the guest
      // reuploads a previously resolved depth buffer to the EDRAM, rounding
      // towards zero (which contradicts the rounding used by the Direct3D 9
      // reference rasterizer, but allows SV_DepthLessEqual to be used to allow
      // slightly coarse early Z culling; also truncating regardless of whether
      // the shader writes depth and thus always uses SV_Depth, for
      // consistency). MSAA is limited - depth must be per-sample
      // (SV_DepthLessEqual also explicitly requires sample or centroid position
      // interpolation), thus the sampler has to run at sample frequency even if
      // the device supports stencil loading and thus true non-ROV MSAA via
      // SV_StencilRef.
      // Fixed-function viewport depth bounds must be snapped to float24 for
      // clamping purposes.
      kFloat24Truncating,
      // Similar to kFloat24Truncating, but rounding to the nearest even,
      // however, always using SV_Depth rather than SV_DepthLessEqual because
      // rounding up results in a bigger value. Same viewport usage rules apply.
      kFloat24Rounding,
    };

    struct VertexShaderModification {
      // Dynamically indexable register count from SQ_PROGRAM_CNTL.
      uint32_t dynamic_addressable_register_count : 8;
      // Pipeline stage and input configuration.
      Shader::HostVertexShaderType host_vertex_shader_type
          : Shader::kHostVertexShaderTypeBitCount;
    } vertex;
    struct PixelShaderModification {
      // Dynamically indexable register count from SQ_PROGRAM_CNTL.
      uint32_t dynamic_addressable_register_count : 8;
      // Non-ROV - depth / stencil output mode.
      DepthStencilMode depth_stencil_mode : 2;
    } pixel;
    uint64_t value = 0;

    Modification(uint64_t modification_value = 0) : value(modification_value) {}
  };

  // Constant buffer bindings in space 0.
  enum class CbufferRegister {
    kSystemConstants,
    kFloatConstants,
    kBoolLoopConstants,
    kFetchConstants,
    kDescriptorIndices,
  };

  // Some are referenced in xenos_draw.hlsli - check it too when updating!
  enum : uint32_t {
    kSysFlag_SharedMemoryIsUAV_Shift,
    kSysFlag_XYDividedByW_Shift,
    kSysFlag_ZDividedByW_Shift,
    kSysFlag_WNotReciprocal_Shift,
    kSysFlag_UserClipPlane0_Shift,
    kSysFlag_UserClipPlane1_Shift,
    kSysFlag_UserClipPlane2_Shift,
    kSysFlag_UserClipPlane3_Shift,
    kSysFlag_UserClipPlane4_Shift,
    kSysFlag_UserClipPlane5_Shift,
    kSysFlag_KillIfAnyVertexKilled_Shift,
    kSysFlag_PrimitivePolygonal_Shift,
    kSysFlag_DepthFloat24_Shift,
    kSysFlag_AlphaPassIfLess_Shift,
    kSysFlag_AlphaPassIfEqual_Shift,
    kSysFlag_AlphaPassIfGreater_Shift,
    kSysFlag_ConvertColor0ToGamma_Shift,
    kSysFlag_ConvertColor1ToGamma_Shift,
    kSysFlag_ConvertColor2ToGamma_Shift,
    kSysFlag_ConvertColor3ToGamma_Shift,

    kSysFlag_ROVDepthStencil_Shift,
    kSysFlag_ROVDepthPassIfLess_Shift,
    kSysFlag_ROVDepthPassIfEqual_Shift,
    kSysFlag_ROVDepthPassIfGreater_Shift,
    // 1 to write new depth to the depth buffer, 0 to keep the old one if the
    // depth test passes.
    kSysFlag_ROVDepthWrite_Shift,
    kSysFlag_ROVStencilTest_Shift,
    // If the depth / stencil test has failed, but resulted in a stencil value
    // that is different than the one currently in the depth buffer, write it
    // anyway and don't run the rest of the shader (to check if the sample may
    // be discarded some way) - use when alpha test and alpha to coverage are
    // disabled. Ignored by the shader if not applicable to it (like if it has
    // kill instructions or writes the depth output).
    // TODO(Triang3l): Investigate replacement with an alpha-to-mask flag,
    // checking `(flags & (alpha test | alpha to mask)) == (always | disabled)`,
    // taking into account the potential relation with occlusion queries (but
    // should be safe at least temporarily).
    kSysFlag_ROVDepthStencilEarlyWrite_Shift,

    kSysFlag_Count,

    kSysFlag_SharedMemoryIsUAV = 1u << kSysFlag_SharedMemoryIsUAV_Shift,
    kSysFlag_XYDividedByW = 1u << kSysFlag_XYDividedByW_Shift,
    kSysFlag_ZDividedByW = 1u << kSysFlag_ZDividedByW_Shift,
    kSysFlag_WNotReciprocal = 1u << kSysFlag_WNotReciprocal_Shift,
    kSysFlag_UserClipPlane0 = 1u << kSysFlag_UserClipPlane0_Shift,
    kSysFlag_UserClipPlane1 = 1u << kSysFlag_UserClipPlane1_Shift,
    kSysFlag_UserClipPlane2 = 1u << kSysFlag_UserClipPlane2_Shift,
    kSysFlag_UserClipPlane3 = 1u << kSysFlag_UserClipPlane3_Shift,
    kSysFlag_UserClipPlane4 = 1u << kSysFlag_UserClipPlane4_Shift,
    kSysFlag_UserClipPlane5 = 1u << kSysFlag_UserClipPlane5_Shift,
    kSysFlag_KillIfAnyVertexKilled = 1u << kSysFlag_KillIfAnyVertexKilled_Shift,
    kSysFlag_PrimitivePolygonal = 1u << kSysFlag_PrimitivePolygonal_Shift,
    kSysFlag_DepthFloat24 = 1u << kSysFlag_DepthFloat24_Shift,
    kSysFlag_AlphaPassIfLess = 1u << kSysFlag_AlphaPassIfLess_Shift,
    kSysFlag_AlphaPassIfEqual = 1u << kSysFlag_AlphaPassIfEqual_Shift,
    kSysFlag_AlphaPassIfGreater = 1u << kSysFlag_AlphaPassIfGreater_Shift,
    kSysFlag_ConvertColor0ToGamma = 1u << kSysFlag_ConvertColor0ToGamma_Shift,
    kSysFlag_ConvertColor1ToGamma = 1u << kSysFlag_ConvertColor1ToGamma_Shift,
    kSysFlag_ConvertColor2ToGamma = 1u << kSysFlag_ConvertColor2ToGamma_Shift,
    kSysFlag_ConvertColor3ToGamma = 1u << kSysFlag_ConvertColor3ToGamma_Shift,
    kSysFlag_ROVDepthStencil = 1u << kSysFlag_ROVDepthStencil_Shift,
    kSysFlag_ROVDepthPassIfLess = 1u << kSysFlag_ROVDepthPassIfLess_Shift,
    kSysFlag_ROVDepthPassIfEqual = 1u << kSysFlag_ROVDepthPassIfEqual_Shift,
    kSysFlag_ROVDepthPassIfGreater = 1u << kSysFlag_ROVDepthPassIfGreater_Shift,
    kSysFlag_ROVDepthWrite = 1u << kSysFlag_ROVDepthWrite_Shift,
    kSysFlag_ROVStencilTest = 1u << kSysFlag_ROVStencilTest_Shift,
    kSysFlag_ROVDepthStencilEarlyWrite =
        1u << kSysFlag_ROVDepthStencilEarlyWrite_Shift,
  };
  static_assert(kSysFlag_Count <= 32, "Too many flags in the system constants");

  // Appended to the format in the format constant.
  enum : uint32_t {
    // Starting from bit 4 because the format itself needs 4 bits.
    kRTFormatFlag_64bpp_Shift = 4,
    // Requires clamping of blending sources and factors.
    kRTFormatFlag_FixedPointColor_Shift,
    kRTFormatFlag_FixedPointAlpha_Shift,

    kRTFormatFlag_64bpp = 1u << kRTFormatFlag_64bpp_Shift,
    kRTFormatFlag_FixedPointColor = 1u << kRTFormatFlag_FixedPointColor_Shift,
    kRTFormatFlag_FixedPointAlpha = 1u << kRTFormatFlag_FixedPointAlpha_Shift,
  };

  // IF SYSTEM CONSTANTS ARE CHANGED OR ADDED, THE FOLLOWING MUST BE UPDATED:
  // - SystemConstantIndex enum.
  // - system_constant_rdef_.
  // - d3d12/shaders/xenos_draw.hlsli (for geometry shaders).
  struct SystemConstants {
    uint32_t flags;
    union {
      struct {
        float tessellation_factor_range_min;
        float tessellation_factor_range_max;
      };
      float tessellation_factor_range[2];
    };
    uint32_t line_loop_closing_index;

    xenos::Endian vertex_index_endian;
    int32_t vertex_base_index;
    float point_size[2];

    float point_size_min_max[2];
    // Screen point size * 2 (but not supersampled) -> size in NDC.
    float point_screen_to_ndc[2];

    float user_clip_planes[6][4];

    float ndc_scale[3];
    uint32_t interpolator_sampling_pattern;

    float ndc_offset[3];
    uint32_t ps_param_gen;

    // Each byte contains post-swizzle TextureSign values for each of the needed
    // components of each of the 32 used texture fetch constants.
    uint32_t texture_swizzled_signs[8];

    // Whether the contents of each texture in fetch constants comes from a
    // resolve operation.
    uint32_t textures_resolved;
    // Log2 of X and Y sample size. Used for alpha to mask, and for MSAA with
    // ROV, this is used for EDRAM address calculation.
    uint32_t sample_count_log2[2];
    float alpha_test_reference;

    float color_exp_bias[4];

    // If alpha to mask is disabled, the entire alpha_to_mask value must be 0.
    // If alpha to mask is enabled, bits 0:7 are sample offsets, and bit 8 must
    // be 1.
    uint32_t alpha_to_mask;
    uint32_t edram_pitch_tiles;
    union {
      struct {
        float edram_depth_range_scale;
        float edram_depth_range_offset;
      };
      float edram_depth_range[2];
    };

    union {
      struct {
        float edram_poly_offset_front_scale;
        float edram_poly_offset_front_offset;
      };
      float edram_poly_offset_front[2];
    };
    union {
      struct {
        float edram_poly_offset_back_scale;
        float edram_poly_offset_back_offset;
      };
      float edram_poly_offset_back[2];
    };

    uint32_t edram_depth_base_dwords;
    uint32_t padding_edram_depth_base_dwords[3];

    // In stencil function/operations (they match the layout of the
    // function/operations in RB_DEPTHCONTROL):
    // 0:2 - comparison function (bit 0 - less, bit 1 - equal, bit 2 - greater).
    // 3:5 - fail operation.
    // 6:8 - pass operation.
    // 9:11 - depth fail operation.

    union {
      struct {
        uint32_t edram_stencil_front_reference;
        uint32_t edram_stencil_front_read_mask;
        uint32_t edram_stencil_front_write_mask;
        uint32_t edram_stencil_front_func_ops;

        uint32_t edram_stencil_back_reference;
        uint32_t edram_stencil_back_read_mask;
        uint32_t edram_stencil_back_write_mask;
        uint32_t edram_stencil_back_func_ops;
      };
      struct {
        uint32_t edram_stencil_front[4];
        uint32_t edram_stencil_back[4];
      };
      uint32_t edram_stencil[2][4];
    };

    uint32_t edram_rt_base_dwords_scaled[4];

    // RT format combined with kRTFormatFlags.
    uint32_t edram_rt_format_flags[4];

    // Format info - values to clamp the color to before blending or storing.
    // Low color, low alpha, high color, high alpha.
    float edram_rt_clamp[4][4];

    // Format info - mask to apply to the old packed RT data, and to apply as
    // inverted to the new packed data, before storing (more or less the inverse
    // of the write mask packed like render target channels). This can be used
    // to bypass unpacking if blending is not used. If 0 and not blending,
    // reading the old data from the EDRAM buffer is not required.
    uint32_t edram_rt_keep_mask[4][2];

    // Render target blending options - RB_BLENDCONTROL, with only the relevant
    // options (factors and operations - AND 0x1FFF1FFF). If 0x00010001
    // (1 * src + 0 * dst), blending is disabled for the render target.
    uint32_t edram_rt_blend_factors_ops[4];

    // The constant blend factor for the respective modes.
    float edram_blend_constant[4];
  };

  // Shader resource view binding spaces.
  enum class SRVSpace {
    // SRVMainSpaceRegister t# layout.
    kMain,
    kBindlessTextures2DArray,
    kBindlessTextures3D,
    kBindlessTexturesCube,
  };

  // Shader resource view bindings in SRVSpace::kMain.
  enum class SRVMainRegister {
    kSharedMemory,
    kBindfulTexturesStart,
  };

  // 192 textures at most because there are 32 fetch constants, and textures can
  // be 2D array, 3D or cube, and also signed and unsigned.
  static constexpr uint32_t kMaxTextureBindingIndexBits = 8;
  static constexpr uint32_t kMaxTextureBindings =
      (1 << kMaxTextureBindingIndexBits) - 1;
  struct TextureBinding {
    uint32_t bindful_srv_index;
    // Temporary for WriteResourceDefinition.
    uint32_t bindful_srv_rdef_name_ptr;
    uint32_t bindless_descriptor_index;
    uint32_t fetch_constant;
    // Stacked and 3D are separate TextureBindings, even for bindless for null
    // descriptor handling simplicity.
    xenos::FetchOpDimension dimension;
    bool is_signed;
    std::string name;
  };

  // Arbitrary limit - there can't be more than 2048 in a shader-visible
  // descriptor heap, though some older hardware (tier 1 resource binding -
  // Nvidia Fermi) doesn't support more than 16 samplers bound at once (we can't
  // really do anything if a game uses more than 16), but just to have some
  // limit so sampler count can easily be packed into 32-bit map keys (for
  // instance, for root signatures). But shaders can specify overrides for
  // filtering modes, and the number of possible combinations is huge - let's
  // limit it to something sane.
  static constexpr uint32_t kMaxSamplerBindingIndexBits = 7;
  static constexpr uint32_t kMaxSamplerBindings =
      (1 << kMaxSamplerBindingIndexBits) - 1;
  struct SamplerBinding {
    uint32_t bindless_descriptor_index;
    uint32_t fetch_constant;
    xenos::TextureFilter mag_filter;
    xenos::TextureFilter min_filter;
    xenos::TextureFilter mip_filter;
    xenos::AnisoFilter aniso_filter;
    std::string name;
  };

  // Unordered access view bindings in space 0.
  enum class UAVRegister {
    kSharedMemory,
    kEdram,
  };

  // Returns the format with internal flags for passing via the
  // edram_rt_format_flags system constant.
  static constexpr uint32_t ROV_AddColorFormatFlags(
      xenos::ColorRenderTargetFormat format) {
    uint32_t format_flags = uint32_t(format);
    if (format == xenos::ColorRenderTargetFormat::k_16_16_16_16 ||
        format == xenos::ColorRenderTargetFormat::k_16_16_16_16_FLOAT ||
        format == xenos::ColorRenderTargetFormat::k_32_32_FLOAT) {
      format_flags |= kRTFormatFlag_64bpp;
    }
    if (format == xenos::ColorRenderTargetFormat::k_8_8_8_8 ||
        format == xenos::ColorRenderTargetFormat::k_8_8_8_8_GAMMA ||
        format == xenos::ColorRenderTargetFormat::k_2_10_10_10 ||
        format == xenos::ColorRenderTargetFormat::k_16_16 ||
        format == xenos::ColorRenderTargetFormat::k_16_16_16_16 ||
        format == xenos::ColorRenderTargetFormat::k_2_10_10_10_AS_10_10_10_10) {
      format_flags |=
          kRTFormatFlag_FixedPointColor | kRTFormatFlag_FixedPointAlpha;
    } else if (format == xenos::ColorRenderTargetFormat::k_2_10_10_10_FLOAT ||
               format == xenos::ColorRenderTargetFormat::
                             k_2_10_10_10_FLOAT_AS_16_16_16_16) {
      format_flags |= kRTFormatFlag_FixedPointAlpha;
    }
    return format_flags;
  }
  // Returns the bits that need to be added to the RT flags constant - needs to
  // be done externally, not in SetColorFormatConstants, because the flags
  // contain other state.
  static void ROV_GetColorFormatSystemConstants(
      xenos::ColorRenderTargetFormat format, uint32_t write_mask,
      float& clamp_rgb_low, float& clamp_alpha_low, float& clamp_rgb_high,
      float& clamp_alpha_high, uint32_t& keep_mask_low,
      uint32_t& keep_mask_high);

  uint64_t GetDefaultVertexShaderModification(
      uint32_t dynamic_addressable_register_count,
      Shader::HostVertexShaderType host_vertex_shader_type =
          Shader::HostVertexShaderType::kVertex) const override;
  uint64_t GetDefaultPixelShaderModification(
      uint32_t dynamic_addressable_register_count) const override;

  // Creates a special pixel shader without color outputs - this resets the
  // state of the translator.
  std::vector<uint8_t> CreateDepthOnlyPixelShader();

  // Common functions useful not only for the translator, but also for render
  // target reinterpretation.

  // Converts the color value externally clamped to [0, 31.875] to 7e3 floating
  // point, with zeros in bits 10:31, rounding to the nearest even. Source and
  // destination may be the same, temporary must be different than both.
  static void PreClampedFloat32To7e3(dxbc::Assembler& a, uint32_t f10_temp,
                                     uint32_t f10_temp_component,
                                     uint32_t f32_temp,
                                     uint32_t f32_temp_component,
                                     uint32_t temp_temp,
                                     uint32_t temp_temp_component);
  // Same as PreClampedFloat32To7e3, but clamps the input to [0, 31.875].
  static void UnclampedFloat32To7e3(dxbc::Assembler& a, uint32_t f10_temp,
                                    uint32_t f10_temp_component,
                                    uint32_t f32_temp,
                                    uint32_t f32_temp_component,
                                    uint32_t temp_temp,
                                    uint32_t temp_temp_component);
  // Converts the 7e3 number in bits [f10_shift, f10_shift + 10) to a 32-bit
  // float. Two temporaries must be different, but one can be the same as the
  // source. The destination may be anything writable.
  static void Float7e3To32(dxbc::Assembler& a, const dxbc::Dest& f32,
                           uint32_t f10_temp, uint32_t f10_temp_component,
                           uint32_t f10_shift, uint32_t temp1_temp,
                           uint32_t temp1_temp_component, uint32_t temp2_temp,
                           uint32_t temp2_temp_component);
  // Converts the depth value externally clamped to the representable [0, 2)
  // range to 20e4 floating point, with zeros in bits 24:31, rounding to the
  // nearest even. Source and destination may be the same, temporary must be
  // different than both. If remap_from_0_to_0_5 is true, it's assumed that
  // 0...1 is pre-remapped to 0...0.5 on the input.
  static void PreClampedDepthTo20e4(
      dxbc::Assembler& a, uint32_t f24_temp, uint32_t f24_temp_component,
      uint32_t f32_temp, uint32_t f32_temp_component, uint32_t temp_temp,
      uint32_t temp_temp_component, bool remap_from_0_to_0_5);
  // Converts the 20e4 number in bits [f24_shift, f24_shift + 10) to a 32-bit
  // float. Two temporaries must be different, but one can be the same as the
  // source. The destination may be anything writable. If remap_to_0_to_0_5 is
  // true, 0...1 in float24 will be remaped to 0...0.5 in float32.
  static void Depth20e4To32(dxbc::Assembler& a, const dxbc::Dest& f32,
                            uint32_t f24_temp, uint32_t f24_temp_component,
                            uint32_t f24_shift, uint32_t temp1_temp,
                            uint32_t temp1_temp_component, uint32_t temp2_temp,
                            uint32_t temp2_temp_component,
                            bool remap_to_0_to_0_5);

 protected:
  void Reset() override;

  uint32_t GetModificationRegisterCount() const override;

  void StartTranslation() override;
  std::vector<uint8_t> CompleteTranslation() override;
  void PostTranslation() override;

  void ProcessLabel(uint32_t cf_index) override;

  void ProcessExecInstructionBegin(const ParsedExecInstruction& instr) override;
  void ProcessExecInstructionEnd(const ParsedExecInstruction& instr) override;
  void ProcessLoopStartInstruction(
      const ParsedLoopStartInstruction& instr) override;
  void ProcessLoopEndInstruction(
      const ParsedLoopEndInstruction& instr) override;
  void ProcessJumpInstruction(const ParsedJumpInstruction& instr) override;
  void ProcessAllocInstruction(const ParsedAllocInstruction& instr) override;

  void ProcessVertexFetchInstruction(
      const ParsedVertexFetchInstruction& instr) override;
  void ProcessTextureFetchInstruction(
      const ParsedTextureFetchInstruction& instr) override;
  void ProcessAluInstruction(const ParsedAluInstruction& instr) override;

 private:
  enum class SystemConstantIndex : uint32_t {
    kFlags,
    kTessellationFactorRange,
    kLineLoopClosingIndex,
    kVertexIndexEndian,
    kVertexBaseIndex,
    kPointSize,
    kPointSizeMinMax,
    kPointScreenToNDC,
    kUserClipPlanes,
    kNDCScale,
    kInterpolatorSamplingPattern,
    kNDCOffset,
    kPSParamGen,
    kTextureSwizzledSigns,
    kTexturesResolved,
    kSampleCountLog2,
    kAlphaTestReference,
    kColorExpBias,
    kAlphaToMask,
    kEdramPitchTiles,
    kEdramDepthRange,
    kEdramPolyOffsetFront,
    kEdramPolyOffsetBack,
    kEdramDepthBaseDwords,
    kEdramStencil,
    kEdramRTBaseDwordsScaled,
    kEdramRTFormatFlags,
    kEdramRTClamp,
    kEdramRTKeepMask,
    kEdramRTBlendFactorsOps,
    kEdramBlendConstant,

    kCount,
  };
  static_assert(uint32_t(SystemConstantIndex::kCount) <= 64,
                "Too many system constants, can't use uint64_t for usage bits");

  static constexpr uint32_t kPointParametersTexCoord = xenos::kMaxInterpolators;
  static constexpr uint32_t kClipSpaceZWTexCoord = kPointParametersTexCoord + 1;

  enum class InOutRegister : uint32_t {
    // IF ANY OF THESE ARE CHANGED, WriteInputSignature and WriteOutputSignature
    // MUST BE UPDATED!
    kVSInVertexIndex = 0,

    kDSInControlPointIndex = 0,

    kVSDSOutInterpolators = 0,
    kVSDSOutPointParameters = kVSDSOutInterpolators + xenos::kMaxInterpolators,
    kVSDSOutClipSpaceZW,
    kVSDSOutPosition,
    // Clip and cull distances must be tightly packed in Direct3D!
    kVSDSOutClipDistance0123,
    kVSDSOutClipDistance45AndCullDistance,
    // TODO(Triang3l): Use SV_CullDistance instead for
    // PA_CL_CLIP_CNTL::UCP_CULL_ONLY_ENA, but can't have more than 8 clip and
    // cull distances in total. Currently only using SV_CullDistance for vertex
    // kill.

    kPSInInterpolators = 0,
    kPSInPointParameters = kPSInInterpolators + xenos::kMaxInterpolators,
    kPSInClipSpaceZW,
    kPSInPosition,
    // nointerpolation inputs. SV_IsFrontFace (X) is always present for
    // ps_param_gen, SV_SampleIndex (Y) is conditional (only for memexport when
    // sample-rate shading is otherwise needed anyway due to depth conversion).
    kPSInFrontFaceAndSampleIndex,
  };

  // GetSystemConstantSrc + MarkSystemConstantUsed is for special cases of
  // building the source unconditionally - in general, LoadSystemConstant must
  // be used instead.
  void MarkSystemConstantUsed(SystemConstantIndex index) {
    system_constants_used_ |= uint64_t(1) << uint32_t(index);
  }
  // Offset should be offsetof(SystemConstants, field). Swizzle values are
  // relative to the first component in the vector according to offsetof - to
  // request a scalar, use XXXX swizzle, and if it's at +4 in its 16-byte
  // vector, it will be turned into YYYY, and so on. The swizzle may include
  // out-of-bounds components of the vector for simplicity of use, assuming they
  // will be dropped anyway later.
  dxbc::Src GetSystemConstantSrc(size_t offset, uint32_t swizzle) {
    uint32_t first_component = uint32_t((offset >> 2) & 3);
    return dxbc::Src::CB(
        cbuffer_index_system_constants_,
        uint32_t(CbufferRegister::kSystemConstants), uint32_t(offset >> 4),
        std::min((swizzle & 3) + first_component, uint32_t(3)) |
            std::min(((swizzle >> 2) & 3) + first_component, uint32_t(3)) << 2 |
            std::min(((swizzle >> 4) & 3) + first_component, uint32_t(3)) << 4 |
            std::min(((swizzle >> 6) & 3) + first_component, uint32_t(3)) << 6);
  }
  dxbc::Src LoadSystemConstant(SystemConstantIndex index, size_t offset,
                               uint32_t swizzle) {
    MarkSystemConstantUsed(index);
    return GetSystemConstantSrc(offset, swizzle);
  }
  dxbc::Src LoadFlagsSystemConstant() {
    return LoadSystemConstant(SystemConstantIndex::kFlags,
                              offsetof(SystemConstants, flags),
                              dxbc::Src::kXXXX);
  }

  Modification GetDxbcShaderModification() const {
    return Modification(current_translation().modification());
  }

  bool IsDxbcVertexShader() const {
    return is_vertex_shader() &&
           GetDxbcShaderModification().vertex.host_vertex_shader_type ==
               Shader::HostVertexShaderType::kVertex;
  }
  bool IsDxbcDomainShader() const {
    return is_vertex_shader() &&
           GetDxbcShaderModification().vertex.host_vertex_shader_type !=
               Shader::HostVertexShaderType::kVertex;
  }

  // Whether to use switch-case rather than if (pc >= label) for control flow.
  bool UseSwitchForControlFlow() const;

  // Allocates new consecutive r# registers for internal use and returns the
  // index of the first.
  uint32_t PushSystemTemp(uint32_t zero_mask = 0, uint32_t count = 1);
  // Frees the last allocated internal r# registers for later reuse.
  void PopSystemTemp(uint32_t count = 1);

  // Converts one scalar to or from PWL gamma, using 1 temporary scalar.
  // The target may be the same as any of the source, the piece temporary or the
  // accumulator, but not two or three of these.
  // The piece and the accumulator can't be the same as source or as each other.
  void ConvertPWLGamma(bool to_gamma, int32_t source_temp,
                       uint32_t source_temp_component, uint32_t target_temp,
                       uint32_t target_temp_component, uint32_t piece_temp,
                       uint32_t piece_temp_component, uint32_t accumulator_temp,
                       uint32_t accumulator_temp_component);

  bool IsSampleRate() const {
    assert_true(is_pixel_shader());
    return DSV_IsWritingFloat24Depth() && !current_shader().writes_depth();
  }
  bool IsDepthStencilSystemTempUsed() const {
    // See system_temp_depth_stencil_ documentation for explanation of cases.
    if (current_shader().writes_depth()) {
      // With host render targets, the depth format may be float24, in this
      // case, need to multiply it by 0.5 since 0...1 of the guest is stored as
      // 0...0.5 on the host, and also to convert it.
      // With ROV, need to store it to write later.
      return true;
    }
    if (edram_rov_used_ && ROV_IsDepthStencilEarly()) {
      // Calculated in the beginning, written possibly in the end.
      return true;
    }
    return false;
  }
  // Whether the current non-ROV pixel shader should convert the depth to 20e4.
  bool DSV_IsWritingFloat24Depth() const {
    if (edram_rov_used_) {
      return false;
    }
    Modification::DepthStencilMode depth_stencil_mode =
        GetDxbcShaderModification().pixel.depth_stencil_mode;
    return depth_stencil_mode ==
               Modification::DepthStencilMode::kFloat24Truncating ||
           depth_stencil_mode ==
               Modification::DepthStencilMode::kFloat24Rounding;
  }
  // Whether it's possible and worth skipping running the translated shader for
  // 2x2 quads.
  bool ROV_IsDepthStencilEarly() const {
    return !is_depth_only_pixel_shader_ && !current_shader().writes_depth() &&
           !current_shader().is_valid_memexport_used();
  }
  // Converts the pre-clamped depth value to 24-bit (storing the result in bits
  // 0:23 and zeros in 24:31, not creating room for stencil - since this may be
  // involved in comparisons) according to the format specified in the system
  // constants. Source and destination may be the same, temporary must be
  // different than both.
  void ROV_DepthTo24Bit(uint32_t d24_temp, uint32_t d24_temp_component,
                        uint32_t d32_temp, uint32_t d32_temp_component,
                        uint32_t temp_temp, uint32_t temp_temp_component);
  // Does all the related to depth / stencil, including or not including
  // writing based on whether it's late, or on whether it's safe to do it early.
  // Updates system_temp_rov_params_ result and coverage if allowed and safe,
  // updates system_temp_depth_stencil_, and if early and the coverage is empty
  // for all pixels in the 2x2 quad and safe to return early (stencil is
  // unchanged or known that it's safe not to await kills/alphatest/AtoC),
  // returns from the shader.
  void ROV_DepthStencilTest();
  // Unpacks a 32bpp or a 64bpp color in packed_temp.packed_temp_components to
  // color_temp, using 2 temporary VGPRs.
  void ROV_UnpackColor(uint32_t rt_index, uint32_t packed_temp,
                       uint32_t packed_temp_components, uint32_t color_temp,
                       uint32_t temp1, uint32_t temp1_component, uint32_t temp2,
                       uint32_t temp2_component);
  // Packs a float32x4 color value to 32bpp or a 64bpp in color_temp to
  // packed_temp.packed_temp_components, using 2 temporary VGPR. color_temp and
  // packed_temp may be the same if packed_temp_components is 0. If the format
  // is 32bpp, will still write the high part to break register dependency.
  void ROV_PackPreClampedColor(uint32_t rt_index, uint32_t color_temp,
                               uint32_t packed_temp,
                               uint32_t packed_temp_components, uint32_t temp1,
                               uint32_t temp1_component, uint32_t temp2,
                               uint32_t temp2_component);
  // Emits a sequence of `case` labels for color blend factors, generating the
  // factor from src_temp.rgb and dst_temp.rgb to factor_temp.rgb. factor_temp
  // can be the same as src_temp or dst_temp.
  void ROV_HandleColorBlendFactorCases(uint32_t src_temp, uint32_t dst_temp,
                                       uint32_t factor_temp);
  // Emits a sequence of `case` labels for alpha blend factors, generating the
  // factor from src_temp.a and dst_temp.a to factor_temp.factor_component.
  // factor_temp can be the same as src_temp or dst_temp.
  void ROV_HandleAlphaBlendFactorCases(uint32_t src_temp, uint32_t dst_temp,
                                       uint32_t factor_temp,
                                       uint32_t factor_component);

  // Writing the prologue.
  void StartVertexShader_LoadVertexIndex();
  void StartVertexOrDomainShader();
  void StartDomainShader();
  void StartPixelShader_LoadROVParameters();
  void StartPixelShader();

  // Writing the epilogue.
  // ExportToMemory modifies the values of eA/eM# for simplicity, don't call
  // multiple times.
  void ExportToMemory_PackFixed32(const uint32_t* eM_temps, uint32_t eM_count,
                                  const uint32_t bits[4],
                                  const dxbc::Src& is_integer,
                                  const dxbc::Src& is_signed);
  void ExportToMemory();
  void CompleteVertexOrDomainShader();
  // For RTV, adds the sample to coverage_temp.coverage_temp_component if it
  // passes alpha to mask (except for sample 0, which overwrites the output to
  // initialize it).
  // For ROV, masks the sample away from coverage_temp.coverage_temp_component
  // if it doesn't pass alpha to mask.
  // threshold_offset and temp.temp_component can be the same if needed.
  void CompletePixelShader_AlphaToMaskSample(
      uint32_t sample_index, float threshold_base, dxbc::Src threshold_offset,
      float threshold_offset_scale, uint32_t coverage_temp,
      uint32_t coverage_temp_component, uint32_t temp, uint32_t temp_component);
  // Performs alpha to coverage if necessary, for RTV, writing to oMask, and for
  // ROV, updating the low (coverage) bits of system_temp_rov_params_.x. Done
  // manually even for RTV to maintain the guest dithering pattern and because
  // alpha can be exponent-biased.
  void CompletePixelShader_AlphaToMask();
  void CompletePixelShader_WriteToRTVs();
  void CompletePixelShader_DSV_DepthTo24Bit();
  void CompletePixelShader_WriteToROV();
  void CompletePixelShader();

  void CompleteShaderCode();

  // Writes the original instruction disassembly in the output DXBC if enabled,
  // as shader messages, from instruction_disassembly_buffer_.
  void EmitInstructionDisassembly();

  // Converts a shader translator source operand to a DXBC emitter operand, or
  // returns a zero literal operand if it's not going to be referenced. This may
  // allocate a temporary register and emit instructions if the operand can't be
  // used directly with most DXBC instructions (like, if it's an indexable GPR),
  // in this case, temp_pushed_out will be set to true, and PopSystemTemp must
  // be done when the operand is not needed anymore.
  dxbc::Src LoadOperand(const InstructionOperand& operand,
                        uint32_t needed_components, bool& temp_pushed_out);
  // Writes the specified source (src must be usable as a vector `mov` source,
  // including to x#) to an instruction storage target.
  // can_store_memexport_address is for safety, to allow only proper MADs with a
  // stream constant to write to eA.
  void StoreResult(const InstructionResult& result, const dxbc::Src& src,
                   bool can_store_memexport_address = false);

  // The nesting of `if` instructions is the following:
  // - pc checks (labels).
  // - exec predicate/bool constant check.
  // - Instruction-level predicate checks.
  // As an optimization, where possible, the DXBC translator tries to merge
  // multiple execs into one, not creating endif/if doing nothing, if the
  // execution condition is the same. This can't be done across labels
  // (obviously) and in case `setp` is done in a predicated exec - in this case,
  // the predicate value in the current exec may not match the predicate value
  // in the next exec.
  // Instruction-level predicate checks are also merged, and until a `setp` is
  // done, if the instruction has the same predicate condition as the exec it is
  // in, no instruction-level predicate `if` is created as well. One exception
  // to the usual way of instruction-level predicate handling is made for
  // instructions involving derivative computation, such as texture fetches with
  // computed LOD. The part involving derivatives is executed disregarding the
  // predication, but the result storing is predicated (this is handled in
  // texture fetch instruction implementation):
  // https://docs.microsoft.com/en-us/windows/desktop/direct3dhlsl/dx9-graphics-reference-asm-ps-registers-output-color

  // Updates the current flow control condition (to be called in the beginning
  // of exec and in jumps), closing the previous conditionals if needed.
  // However, if the condition is not different, the instruction-level predicate
  // `if` also won't be closed - this must be checked separately if needed (for
  // example, in jumps). Also emits the last disassembly written to
  // instruction_disassembly_buffer_ after closing the previous conditional and
  // before opening a new one.
  void UpdateExecConditionalsAndEmitDisassembly(
      ParsedExecInstruction::Type type, uint32_t bool_constant_index,
      bool condition);
  // Closes `if`s opened by exec and instructions within them (but not by
  // labels) and updates the state accordingly.
  void CloseExecConditionals();
  // Opens or reopens the predicate check conditional for the instruction, and
  // emits the last disassembly written to instruction_disassembly_buffer_ after
  // closing the previous predicate conditional and before opening a new one.
  // This should be called before processing a non-control-flow instruction.
  void UpdateInstructionPredicationAndEmitDisassembly(bool predicated,
                                                      bool condition);
  // Closes the instruction-level predicate `if` if it's open, useful if a flow
  // control instruction needs to do some code which needs to respect the exec's
  // conditional, but can't itself be predicated.
  void CloseInstructionPredication();
  void JumpToLabel(uint32_t address);

  uint32_t FindOrAddTextureBinding(uint32_t fetch_constant,
                                   xenos::FetchOpDimension dimension,
                                   bool is_signed);
  uint32_t FindOrAddSamplerBinding(uint32_t fetch_constant,
                                   xenos::TextureFilter mag_filter,
                                   xenos::TextureFilter min_filter,
                                   xenos::TextureFilter mip_filter,
                                   xenos::AnisoFilter aniso_filter);
  // Returns the number of texture SRV and sampler offsets that need to be
  // passed via a constant buffer to the shader.
  uint32_t GetBindlessResourceCount() const {
    return uint32_t(texture_bindings_.size() + sampler_bindings_.size());
  }
  // Marks fetch constants as used by the DXBC shader and returns dxbc::Src
  // for the words 01 (pair 0), 23 (pair 1) or 45 (pair 2) of the texture fetch
  // constant.
  dxbc::Src RequestTextureFetchConstantWordPair(uint32_t fetch_constant_index,
                                                uint32_t pair_index) {
    if (cbuffer_index_fetch_constants_ == kBindingIndexUnallocated) {
      cbuffer_index_fetch_constants_ = cbuffer_count_++;
    }
    uint32_t total_pair_index = fetch_constant_index * 3 + pair_index;
    return dxbc::Src::CB(cbuffer_index_fetch_constants_,
                         uint32_t(CbufferRegister::kFetchConstants),
                         total_pair_index >> 1,
                         (total_pair_index & 1) ? 0b10101110 : 0b00000100);
  }
  dxbc::Src RequestTextureFetchConstantWord(uint32_t fetch_constant_index,
                                            uint32_t word_index) {
    return RequestTextureFetchConstantWordPair(fetch_constant_index,
                                               word_index >> 1)
        .SelectFromSwizzled(word_index & 1);
  }

  void ProcessVectorAluOperation(const ParsedAluInstruction& instr,
                                 uint32_t& result_swizzle,
                                 bool& predicate_written);
  void ProcessScalarAluOperation(const ParsedAluInstruction& instr,
                                 bool& predicate_written);

  void WriteResourceDefinition();
  void WriteInputSignature();
  void WritePatchConstantSignature();
  void WriteOutputSignature();
  void WriteShaderCode();

  // Executable instructions - generated during translation.
  std::vector<uint32_t> shader_code_;
  // Complete shader object, with all the needed blobs and dcl_ instructions -
  // generated in the end of translation.
  std::vector<uint32_t> shader_object_;

  // Optional Direct3D features used by the shader.
  dxbc::ShaderFeatureInfo shader_feature_info_;
  // The statistics blob.
  dxbc::Statistics statistics_;

  // Assembler for shader_code_ and statistics_ (must be placed after them for
  // correct initialization order).
  dxbc::Assembler a_;
  // Assembler for shader_object_ and statistics_, for declarations before the
  // shader code that depend on info gathered during translation (must be placed
  // after them for correct initialization order).
  dxbc::Assembler ao_;

  // Buffer for instruction disassembly comments.
  StringBuffer instruction_disassembly_buffer_;

  // Whether to write comments with the original Xenos instructions to the
  // output.
  bool emit_source_map_;

  // Vendor ID of the GPU manufacturer, for toggling unsupported features.
  ui::GraphicsProvider::GpuVendorID vendor_id_;

  // Whether textures and samplers should be bindless.
  bool bindless_resources_used_;

  // Whether the output merger should be emulated in pixel shaders.
  bool edram_rov_used_;

  // Whether with RTV-based output-merger, k_8_8_8_8_GAMMA render targets are
  // represented as host sRGB.
  bool gamma_render_target_as_srgb_;

  // Whether 2x MSAA is emulated using real 2x MSAA rather than two samples of
  // 4x MSAA.
  bool msaa_2x_supported_;

  // Guest pixel host width / height.
  uint32_t draw_resolution_scale_;

  // Is currently writing the empty depth-only pixel shader, for
  // CompleteTranslation.
  bool is_depth_only_pixel_shader_ = false;

  // Data types used in constants buffers. Listed in dependency order.
  enum class ShaderRdefTypeIndex {
    kFloat,
    kFloat2,
    kFloat3,
    kFloat4,
    kInt,
    kUint,
    kUint2,
    kUint4,
    // Render target clamping ranges.
    kFloat4Array4,
    // User clip planes.
    kFloat4Array6,
    // Float constants - size written dynamically.
    kFloat4ConstantArray,
    // Bool constants, texture signedness, front/back stencil, render target
    // keep masks.
    kUint4Array2,
    // Loop constants.
    kUint4Array8,
    // Fetch constants.
    kUint4Array48,
    // Descriptor indices - size written dynamically.
    kUint4DescriptorIndexArray,

    kCount,
    kUnknown = kCount
  };

  struct ShaderRdefType {
    // Name ignored for arrays.
    const char* name;
    dxbc::RdefVariableClass variable_class;
    dxbc::RdefVariableType variable_type;
    uint16_t row_count;
    uint16_t column_count;
    uint16_t element_count;
    ShaderRdefTypeIndex array_element_type;
  };
  static const ShaderRdefType rdef_types_[size_t(ShaderRdefTypeIndex::kCount)];

  static constexpr uint32_t kBindingIndexUnallocated = UINT32_MAX;

  // Number of constant buffer bindings used in this shader - also used for
  // generation of indices of constant buffers that are optional.
  uint32_t cbuffer_count_;
  uint32_t cbuffer_index_system_constants_;
  uint32_t cbuffer_index_float_constants_;
  uint32_t cbuffer_index_bool_loop_constants_;
  uint32_t cbuffer_index_fetch_constants_;
  uint32_t cbuffer_index_descriptor_indices_;

  struct SystemConstantRdef {
    const char* name;
    ShaderRdefTypeIndex type;
    uint32_t size;
    uint32_t padding_after;
  };
  static const SystemConstantRdef
      system_constant_rdef_[size_t(SystemConstantIndex::kCount)];
  // Mask of system constants (1 << SystemConstantIndex) used in the shader, so
  // the remaining ones can be marked as unused in RDEF.
  uint64_t system_constants_used_;

  // Mask of domain location actually used in the domain shader.
  uint32_t in_domain_location_used_;
  // Whether the primitive ID has been used in the domain shader.
  bool in_primitive_id_used_;
  // Whether InOutRegister::kDSInControlPointIndex has been used in the shader.
  bool in_control_point_index_used_;
  // Mask of the pixel/sample position actually used in the pixel shader.
  uint32_t in_position_used_;
  // Whether the faceness has been used in the pixel shader.
  bool in_front_face_used_;

  // Number of currently allocated Xenia internal r# registers.
  uint32_t system_temp_count_current_;
  // Total maximum number of temporary registers ever used during this
  // translation (for the declaration).
  uint32_t system_temp_count_max_;

  // Position in vertex shaders (because viewport and W transformations can be
  // applied in the end of the shader).
  uint32_t system_temp_position_;
  // Special exports in vertex shaders.
  uint32_t system_temp_point_size_edge_flag_kill_vertex_;
  // ROV only - 4 persistent VGPRs when writing to color targets, 2 VGPRs when
  // not:
  // X - Bit masks:
  // 0:3 - Per-sample coverage at the current stage of the shader's execution.
  //       Affected by things like SV_Coverage, early or late depth / stencil
  //       (always resets bits for failing, no matter if need to defer writing),
  //       alpha to coverage.
  // 4:7 - Depth write deferred mask - when early depth / stencil resulted in a
  //       different value for the sample (like different stencil if the test
  //       failed), but can't write it before running the shader because it's
  //       not known if the sample will be discarded by the shader, alphatest or
  //       AtoC.
  // Early depth / stencil rejection of the pixel is possible when both 0:3 and
  // 4:7 are zero.
  // 8:11 - Whether color buffers have been written to, if not written on the
  //        taken execution path, don't export according to Direct3D 9 register
  //        documentation (some games rely on this behavior).
  // Y - Absolute resolution-scaled EDRAM offset for depth / stencil, in dwords.
  // Z - Base-relative resolution-scaled EDRAM offset for 32bpp color data, in
  //     dwords.
  // W - Base-relative resolution-scaled EDRAM offset for 64bpp color data, in
  //     dwords.
  uint32_t system_temp_rov_params_;
  // Two purposes:
  // - When writing to oDepth: X also used to hold the depth written by the
  //   shader, which, for host render targets, if the depth buffer is float24,
  //   needs to be remapped from guest 0...1 to host 0...0.5 and, if needed,
  //   converted to float24 precision; and for ROV, needs to be written in the
  //   end of the shader.
  // - Otherwise, when using ROV output with ROV_IsDepthStencilEarly being true:
  //   New per-sample depth / stencil values, generated during early
  //   depth / stencil test (actual writing checks coverage bits).
  uint32_t system_temp_depth_stencil_;
  // Up to 4 color outputs in pixel shaders (needs to be readable, because of
  // alpha test, alpha to coverage, exponent bias, gamma, and also for ROV
  // writing).
  uint32_t system_temps_color_[4];

  // Bits containing whether each eM# has been written, for up to 16 streams, or
  // UINT32_MAX if memexport is not used. 8 bits (5 used) for each stream, with
  // 4 `alloc export`s per component.
  uint32_t system_temp_memexport_written_;
  // eA in each `alloc export`, or UINT32_MAX if not used.
  uint32_t system_temps_memexport_address_[Shader::kMaxMemExports];
  // eM# in each `alloc export`, or UINT32_MAX if not used.
  uint32_t system_temps_memexport_data_[Shader::kMaxMemExports][5];

  // Vector ALU or fetch result / scratch (since Xenos write masks can contain
  // swizzles).
  uint32_t system_temp_result_;
  // Temporary register ID for previous scalar result, program counter,
  // predicate and absolute address register.
  uint32_t system_temp_ps_pc_p0_a0_;
  // Loop index stack - .x is the active loop, shifted right to .yzw on push.
  uint32_t system_temp_aL_;
  // Loop counter stack, .x is the active loop. Represents number of times
  // remaining to loop.
  uint32_t system_temp_loop_count_;
  // Explicitly set texture gradients and LOD.
  uint32_t system_temp_grad_h_lod_;
  uint32_t system_temp_grad_v_;

  // The bool constant number containing the condition for the currently
  // processed exec (or the last - unless a label has reset this), or
  // kCfExecBoolConstantNone if it's not checked.
  uint32_t cf_exec_bool_constant_;
  static constexpr uint32_t kCfExecBoolConstantNone = UINT32_MAX;
  // The expected bool constant value in the current exec if
  // cf_exec_bool_constant_ is not kCfExecBoolConstantNone.
  bool cf_exec_bool_constant_condition_;
  // Whether the currently processed exec is executed if a predicate is
  // set/unset.
  bool cf_exec_predicated_;
  // The expected predicated condition if cf_exec_predicated_ is true.
  bool cf_exec_predicate_condition_;
  // Whether an `if` for instruction-level predicate check is currently open.
  bool cf_instruction_predicate_if_open_;
  // The expected predicate condition for the current or the last instruction if
  // cf_exec_instruction_predicated_ is true.
  bool cf_instruction_predicate_condition_;
  // Whether there was a `setp` in the current exec before the current
  // instruction, thus instruction-level predicate value can be different than
  // the exec-level predicate value, and can't merge two execs with the same
  // predicate condition anymore.
  bool cf_exec_predicate_written_;

  // Number of SRV resources used in this shader - also used for generation of
  // indices of SRV resources that are optional.
  uint32_t srv_count_;
  uint32_t srv_index_shared_memory_;
  uint32_t srv_index_bindless_textures_2d_;
  uint32_t srv_index_bindless_textures_3d_;
  uint32_t srv_index_bindless_textures_cube_;

  // The first binding is at t[SRVMainRegister::kBindfulTexturesStart] of space
  // SRVSpace::kMain.
  std::vector<TextureBinding> texture_bindings_;
  std::unordered_map<uint32_t, uint32_t>
      texture_bindings_for_bindful_srv_indices_;

  // Number of UAV resources used in this shader - also used for generation of
  // indices of UAV resources that are optional.
  uint32_t uav_count_;
  uint32_t uav_index_shared_memory_;
  uint32_t uav_index_edram_;

  std::vector<SamplerBinding> sampler_bindings_;

  // Number of `alloc export`s encountered so far in the translation. The index
  // of the current eA/eM# temp register set is this minus 1, if it's not 0.
  uint32_t memexport_alloc_current_count_;
};

}  // namespace gpu
}  // namespace xe

#endif  // XENIA_GPU_DXBC_SHADER_TRANSLATOR_H_
