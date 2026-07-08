// apps/_MyApp_/shaders_slang/billboard_common.slang
// 빌보드 공통 저수준 추출 함수 - 카메라축 + 모델행렬 center/scale.
// 상위 합성(worldPos 조립)은 호출부마다 다르므로(billboard_atlas=flip/roll, healthbar=headOffset)
// 이 모듈에 넣지 않는다(D-4).
module billboard_common;

// view 의 회전만 추출 (orthonormal 가정 - transpose == inverse-rotation, w=0 방향이라 translation 무영향).
public void ExtractCameraAxes(float4x4 view, out float3 right, out float3 up)
{
    right = mul(transpose(view), float4(1.0, 0.0, 0.0, 0.0)).xyz;
    up    = mul(transpose(view), float4(0.0, 1.0, 0.0, 0.0)).xyz;
}

public float3 ExtractModelCenter(float4x4 model)
{
    return mul(model, float4(0.0, 0.0, 0.0, 1.0)).xyz;
}

// axisLocal = float4(1,0,0,0) 또는 float4(0,1,0,0) - 호출부가 축 지정.
public float ExtractModelAxisScale(float4x4 model, float4 axisLocal)
{
    return length(mul(model, axisLocal).xyz);
}
