#version 410 core

// _MyApp_ Transparent (unlit, emissive) FS
// 라이팅 무관 자체발광 — sampler 를 emissive 로 그대로 출력하고 alpha-blend.
// PoliceTape.png 처럼 alpha 채널을 가진 텍스처를 반투명 액터로 렌더할 때 사용.
// Pass::Kind::Transparent 가 blend(SRC_ALPHA, ONE_MINUS_SRC_ALPHA) + depthWrite off + cull off 자동 도출.
// UV(타일링/시간 스크롤)는 VS 가 결정 — 여기선 샘플만.

in vec2 vsTexCoord;

out vec4 fragColor;

uniform sampler2D emissive; // unit 0 — 자체발광 텍스처
uniform vec4 tintColor; // 곱 틴트 (기본 white = 원본 그대로)

void main()
{
        vec4 color = texture(emissive, vsTexCoord);
        // 투명도는 alpha 채널에 있다 — RGB 휘도가 아니라 alpha 로 컷오프해야 한다.
        // (이 텍스처는 투명한데 RGB 가 밝은 픽셀이 많아 휘도 테스트는 그것들을 못 거른다.)
        if (color.a < 0.1)
                discard;
        fragColor = color;
}
