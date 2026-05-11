#version 410 core

in vec3 vsPosition;
in vec3 vsColor;
in vec2 vsTexCoord;
in vec3 vsNormal;

out vec4 fragColor;

struct Material {
        sampler2D diffuse;
        sampler2D specular;
        float shininess;
};

struct Light {
        vec3 position;
        vec3 ambient;
        vec3 diffuse;
        vec3 specular;
};

struct DirLight {
        vec3 direction;
        vec3 ambient, diffuse, specular;
};

struct PointLight {
        vec3 position;
        float c1, c2;
        vec3 ambient, diffuse, specular;
};

struct SpotLight {
        vec3 position;
        vec3 direction;
        float cutOff, outerCutOff;
        float c1, c2;
        vec3 ambient, diffuse, specular;
};

#define NUM_POINT_LIGHTS 2

uniform Material material;
uniform DirLight dirLight;
uniform PointLight pointLights[NUM_POINT_LIGHTS]; // GLSL은 동적 배열 불가
uniform SpotLight spotLight;
uniform Light light;

uniform vec3 viewPos;
uniform vec3 objectColor;

/*
| 커버 Distance | $c_1$ | $c_2$ |
|--------------|-------|-------|
| 7            | 0.7   | 1.8   |
| 20           | 0.22  | 0.2   |
| 50           | 0.09  | 0.032 |
| 100          | 0.045 | 0.0075|
| 200          | 0.022 | 0.0019|
| 600          | 0.007 | 0.0002|
| 3250         | 0.0014| 0.000007 |
*/
float CalcAttenuation(vec2 lightCoeff, float d)
{
        float c1 = lightCoeff.x;
        float c2 = lightCoeff.y;
        return 1.0 / (1.0 + c1 * d + c2 * d * d);
}

vec3 CalcPhongLight(Light light, vec3 normal, vec3 viewDir, vec3 objectColor);
vec3 CalcDirLight(DirLight light, vec3 normal, vec3 viewDir, vec3 objectColor);
vec3 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 objectColor);
vec3 CalcSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 objectColor);

void main()
{
        vec3 viewDir = normalize(viewPos - vsPosition);
        vec3 result = CalcPhongLight(light, vsNormal, viewDir, objectColor);
        fragColor = vec4(result, 1.0);
}

vec3 CalcPhongLight(Light light, vec3 normal, vec3 viewDir, vec3 objectColor) {
        // ambient
        vec3 diffuseTextureColor = vec3(texture(material.diffuse, vsTexCoord));
        vec3 ambient = diffuseTextureColor * light.ambient;

        // diffuse
        vec3 lightDir = normalize(light.position - vsPosition);
        vec3 pixelNorm = normalize(normal);

        float diff = max(dot(pixelNorm, lightDir), 0.0);
        vec3 diffuse = diff * diffuseTextureColor * light.diffuse;

        // specular
        vec3 specTextureColor = texture(material.specular, vsTexCoord).rgb;
        vec3 reflectDir = reflect(-lightDir, pixelNorm);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
        vec3 specular = spec * specTextureColor * light.specular;
        return (ambient + diffuse + specular) * objectColor;
}

vec3 CalcDirLight(DirLight light, vec3 normal, vec3 viewDir, vec3 objectColor) {
        // ambient
        vec3 diffuseTextureColor = vec3(texture(material.diffuse, vsTexCoord));
        vec3 ambient = light.ambient * diffuseTextureColor;

        // diffuse
        vec3 lightDir = normalize(-light.direction);
        float diff = max(dot(normal, lightDir), 0.0);
        vec3 diffuse = light.diffuse * diff * diffuseTextureColor;

        // specular
        vec3 reflectDir = reflect(-lightDir, normal);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
        vec3 specularTextureColor = vec3(texture(material.specular, vsTexCoord));
        vec3 specular = light.specular * spec * specularTextureColor;

        return (ambient + diffuse + specular) * objectColor; // attenuation 없음
}

vec3 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 objectColor) {
        // ambient
        vec3 diffuseTextureColor = vec3(texture(material.diffuse, vsTexCoord)); // 여기는 동일
        vec3 ambient = light.ambient * diffuseTextureColor; // 여기는 동일

        // diffuse
        vec3 lightDir = normalize(light.position - fragPos); // !
        float diff = max(dot(normal, lightDir), 0.0); // 여기는 동일
        vec3 diffuse = light.diffuse * diff * diffuseTextureColor; // 여기는 동일

        // specular
        vec3 reflectDir = reflect(-lightDir, normal); // 여기는 동일
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess); // 여기는 동일
        vec3 specularTextureColor = vec3(texture(material.specular, vsTexCoord)); // 여기는 동일
        vec3 specular = light.specular * spec * specularTextureColor; // 여기는 동일

        // attenuation (감쇠) 공식
        float distance = length(light.position - fragPos);
        float attenuation = CalcAttenuation(vec2(light.c1, light.c2), distance);

        return (ambient + diffuse + specular) * attenuation * objectColor;
}

vec3 CalcSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir, vec3 objectColor) {
        // ambient
        vec3 diffuseTextureColor = vec3(texture(material.diffuse, vsTexCoord)); // 여기는 동일
        vec3 ambient = light.ambient * diffuseTextureColor; // 여기는 동일

        vec3 lightDir = normalize(light.position - fragPos); // !
        float diff = max(dot(normal, lightDir), 0.0); // 여기는 동일
        vec3 diffuse = light.diffuse * diff * diffuseTextureColor;

        // specular
        vec3 reflectDir = reflect(-lightDir, normal); // 여기는 동일
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess); // 여기는 동일
        vec3 specularTextureColor = vec3(texture(material.specular, vsTexCoord)); // 여기는 동일
        vec3 specular = light.specular * spec * specularTextureColor; // 여기는 동일

        // 소프트 에지
        float theta = dot(lightDir, normalize(-light.direction));
        float epsilon = light.cutOff - light.outerCutOff;
        float intensity = clamp((theta - light.outerCutOff) / epsilon, 0.0, 1.0);

        // attenuation (감쇠) 공식
        float distance = length(light.position - fragPos);
        float attenuation = CalcAttenuation(vec2(light.c1, light.c2), distance);

        // 세 요소 모두에 attenuation × intensity 곱
        ambient *= attenuation * intensity;
        diffuse *= attenuation * intensity;
        specular *= attenuation * intensity;
        return (ambient + diffuse + specular) * objectColor;
}
