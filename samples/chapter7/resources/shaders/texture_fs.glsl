// #version 430 core
#version 410 core

out vec4 outBufferColor;

uniform vec4 inBaseColor;

uniform sampler2D tex1;
uniform sampler2D tex2;
uniform sampler2D tex3;
uniform sampler2D tex4;

uniform float uTex1Used;
uniform float uTex2Used;
uniform float uTex3Used;
uniform float uTex4Used;

uniform vec2 inUVOffset1;
uniform vec2 inUVOffset2;
uniform vec2 inUVOffset3;
uniform vec2 inUVOffset4;

uniform vec2 inUVRatio1;
uniform vec2 inUVRatio2;
uniform vec2 inUVRatio3;
uniform vec2 inUVRatio4;

uniform vec3 inLightPos; // 월드 원점 기준으로 본 라이트 위치 -> 정규화하면 light 방향
uniform vec3 inLightColor; // 광원 RGB
uniform vec3 inViewPos; // 월드 원점 기준 카메라 위치 -> 정규화하면 view 방향
uniform float inAmbientStrength; // 0~1
uniform float inSpecularStrength; // 0~1
uniform float inShininess;
uniform float inLightingEnabled; // 0 이면 라이팅 OFF (텍스처 그대로)

in outVertexData {
        vec4 color;
        vec2 uvCoord;
        vec3 normal;
} fs_in;

void main(void) {
        vec4 resColor = vec4(1.0);

        if (uTex1Used >= 0.99) {
                vec2 uv = inUVOffset1 + fs_in.uvCoord * inUVRatio1;
                vec4 layer = texture(tex1, uv);
                float opacity = layer.a;
                if (opacity > 0.05)
                        resColor = layer * opacity + resColor * (1.0 - opacity); // 가중치 블랜딩
                else // 첫번쨰가 알파면 그냥 투명하게 무조건 그리자.
                        discard; // 첫번쨰가 알파면 그냥 투명하게 무조건 그리자.
        }
        if (uTex2Used >= 0.99) {
                vec2 uv = inUVOffset2 + fs_in.uvCoord * inUVRatio2;
                vec4 layer = texture(tex2, uv);
                float opacity = layer.a;
                if (opacity > 0.05)
                        resColor = layer * opacity + resColor * (1.0 - opacity); // 가중치 블랜딩
                // else
                //         discard;
        }
        if (uTex3Used >= 0.99) {
                vec2 uv = inUVOffset3 + fs_in.uvCoord * inUVRatio3;
                vec4 layer = texture(tex3, uv);
                float opacity = layer.a;
                if (opacity > 0.05)
                        resColor = layer * opacity + resColor * (1.0 - opacity); // 가중치 블랜딩
                // else
                //         discard;
        }
        if (uTex4Used >= 0.99) {
                vec2 uv = inUVOffset4 + fs_in.uvCoord * inUVRatio4;
                vec4 layer = texture(tex4, uv);
                float opacity = layer.a;
                if (opacity > 0.05)
                        resColor = layer * opacity + resColor * (1.0 - opacity); // 가중치 블랜딩
                // else
                //         discard;
        }

        vec4 albedo = inBaseColor * fs_in.color * resColor;

        if (inLightingEnabled < 0.5) {
                // 라이팅 OFF — 텍스처/색만 출력
                outBufferColor = albedo;
                return;
        }

        vec3 N = normalize(fs_in.normal);
        vec3 L = normalize(inLightPos); // light 방향
        vec3 V = normalize(inViewPos); // view 방향
        vec3 R = reflect(-L, N); // 입사광이 N 기준으로 반사된 방향

        // Ambient — 주변광. N/L 무관.
        vec3 ambient = inAmbientStrength * inLightColor;

        // Diffuse — Lambert 의 코사인 법칙. N·L 이 0 이하인 면은 빛을 전혀 못 받음.
        float diff = max(dot(N, L), 0.0);
        vec3 diffuse = diff * inLightColor;

        // Specular — Phong : (R·V)^shininess. N·L<=0 이면 specular 도 0 (뒷면 highlight 방지).
        float spec = 0.0;
        if (diff > 0.0)
                spec = pow(max(dot(R, V), 0.0), inShininess);
        vec3 specular = inSpecularStrength * spec * inLightColor;

        vec3 lighting = ambient + diffuse + specular;
        outBufferColor = vec4(lighting, 1.0) * albedo;
}
