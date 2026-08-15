// Offline compile/audit copy of the runtime-embedded terrain motion shader.
// The replay temporarily aliases the domain-stage camera b1 descriptor onto
// the pixel-visible b6 binding already declared by the terrain root signature.
// REDengine's ordinary motion shaders establish current NDC minus history NDC.
cbuffer CameraShaderConsts : register(b6) {
    float4 cameraRows[44];
};
struct PixelInput {
    float4 terrainData0 : TEXCOORD0;
    nointerpolation int4 terrainData1 : TEXCOORD1;
    float3 worldPosition : TEXCOORD2;
    bool frontFace : SV_IsFrontFace;
};
float4 projectWorld(float3 worldPosition, uint firstRow) {
    float4 worldPoint = float4(worldPosition, 1.0);
    return float4(
        dot(cameraRows[firstRow + 0], worldPoint),
        dot(cameraRows[firstRow + 1], worldPoint),
        dot(cameraRows[firstRow + 2], worldPoint),
        dot(cameraRows[firstRow + 3], worldPoint));
}
float4 ps_main(PixelInput input) : SV_Target3 {
    float4 currentClip = projectWorld(input.worldPosition, 0);
    float4 historyClip = projectWorld(input.worldPosition, 4);
    float3 currentNdc = currentClip.xyz / currentClip.w;
    float3 historyNdc = historyClip.xyz / historyClip.w;
    float3 currentMinusHistory = currentNdc - historyNdc;
    return float4(
        currentMinusHistory.x * 0.5,
        currentMinusHistory.y * -0.5,
        currentMinusHistory.z,
        1.0);
}
