// apps/_MyApp_/shaders_slang/image_processing.slang
// 이미지 처리 공통 - 휘도 / 가우시안 가중치 / 텍셀 크기 / 3x3 컨볼루션.
// sampler/배열 param 을 공유 함수로 넘기는 최초 사례 - macOS GLSL410 통과가 전제(스파이크 D2).
module image_processing;

// Rec.709 휘도.
public float Luminance(float3 rgb)
{
    return dot(rgb, float3(0.299, 0.587, 0.114));
}

// 가우시안 가중치 exp(-거리제곱 * falloff).
public float GaussianWeight(float2 offset, float falloff)
{
    return exp(-dot(offset, offset) * falloff);
}

// 샘플러 해상도의 역수 = 1 텍셀의 UV 크기.
public float2 TexelSize(Sampler2D s)
{
    uint w, h;
    s.GetDimensions(w, h);
    return 1.0 / float2(w, h);
}

// 3x3 이웃 단위 오프셋 (texel 곱은 호출부).
public static const float2 kOffset3x3[9] = {
    float2(-1.0,  1.0), float2(0.0,  1.0), float2(1.0,  1.0),
    float2(-1.0,  0.0), float2(0.0,  0.0), float2(1.0,  0.0),
    float2(-1.0, -1.0), float2(0.0, -1.0), float2(1.0, -1.0)
};

// 3x3 이웃 9색 샘플 (uv 중심, texel 간격).
public void Sample3x3(Sampler2D s, float2 uv, float2 texel, out float3 c[9])
{
    for (int i = 0; i < 9; ++i)
        c[i] = s.Sample(uv + kOffset3x3[i] * texel).rgb;
}

// 9스칼라 x 9커널 가중합 (휘도 컨볼루션 - sobel gx/gy).
public float Dot9(float l[9], float k[9])
{
    float sum = 0.0;
    for (int i = 0; i < 9; ++i) sum += l[i] * k[i];
    return sum;
}

// 9색 x 9커널 가중합 (색 컨볼루션 - sharpening).
public float3 Dot9Color(float3 c[9], float k[9])
{
    float3 sum = float3(0.0, 0.0, 0.0);
    for (int i = 0; i < 9; ++i) sum += c[i] * k[i];
    return sum;
}
