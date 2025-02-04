#ifndef XENIA_GPU_D3D12_SHADERS_HOST_DEPTH_STORE_HLSLI_
#define XENIA_GPU_D3D12_SHADERS_HOST_DEPTH_STORE_HLSLI_

cbuffer XeHostDepthStoreRectangleConstants : register(b0) {
  uint xe_host_depth_store_rectangle;
};

cbuffer XeHostDepthStoreRenderTargetConstants : register(b1) {
  uint xe_host_depth_store_render_target;
};
RWBuffer<uint4> xe_host_depth_store_dest : register(u0);

uint2 XeHostDepthStoreResolutionScale() {
  return (xe_host_depth_store_render_target.xx >> uint2(10u, 12u)) & 0x3u;
}

uint2 XeHostDepthStoreUnscaledOrigin() {
  return ((xe_host_depth_store_rectangle.xx >> uint2(0u, 10u)) & 0x3FFu) << 3u;
}

uint2 XeHostDepthStoreScaledOrigin() {
  return XeHostDepthStoreUnscaledOrigin() * XeHostDepthStoreResolutionScale();
}

uint XeHostDepthStoreUnscaledWidthDiv8Minus1() {
  return (xe_host_depth_store_rectangle >> 20u) & 0x3FFu;
}

uint XeHostDepthStoreScaledWidthDiv8() {
  return (XeHostDepthStoreUnscaledWidthDiv8Minus1() + 1u) *
         XeHostDepthStoreResolutionScale().x;
}

// As host depth is needed for at most one transfer destination per update, base
// is not passed to the shader - (0, 0) of the render target is at 0 of the
// destination buffer.

uint XeHostDepthStorePitchTiles() {
  return xe_host_depth_store_render_target & 0x3FFu;
}

bool XeHostDepthStoreMsaa2xSupported() {
  return bool((xe_host_depth_store_render_target >> 14u) & 0x1u);
}

// 40-sample columns are not swapped for addressing simplicity (because this is
// used for depth -> depth transfers, where swapping isn't needed).

#endif  // XENIA_GPU_D3D12_SHADERS_HOST_DEPTH_STORE_HLSLI_