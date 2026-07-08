// apps/_MyApp_/shaders_slang/texture_atlas.slang
// 아틀라스 UV 리맵 공통 - sub-rect 적용 + 균일 그리드 셀 분해.
// ※ CPU SJH::sprite::ComputeUVRect 가 rect 의 SSoT - 이 모듈은 rect 를 grid 로부터 재도출하지 않는다(D-atlas-SSoT).
//   셰이더는 이미 계산된 rect 를 '적용'만 한다.
module texture_atlas;

// 선택된 sub-rect(min,size)로 로컬 [0,1] UV 를 아틀라스 좌표로 리맵.
// rect.xy = (uMin, vMin), rect.zw = (uSize, vSize). billboard_atlas 의 uUvRect 적용식과 동일.
public float2 AtlasSubUV(float2 localUV, float4 rect)
{
    return rect.xy + localUV * rect.zw;
}

// 균일 그리드 셀 분해 - uv*grid 의 floor(셀 인덱스) / frac(셀 내부 [0,1]).
public void AtlasCell(float2 uv, float2 grid, out float2 cellId, out float2 inCell)
{
    float2 g = uv * grid;
    cellId = floor(g);
    inCell = frac(g);
}
