#version 430 core
// #version 410 core

in vec3 vsNormal;
in vec3 vsPos;

uniform vec3 viewPos;
uniform vec3 lightPos;
uniform vec3 lightColor;
uniform vec3 objectColor;
uniform float ambientStrength;
uniform float specularStrength;
uniform float specularShininess;

out vec4 fragColor;

void main()
{
        // 1. 앰비언트
        vec3 ambient = ambientStrength * lightColor;

        // 2. 디퓨즈
        vec3 lightDir = normalize(lightPos - vsPos);
        vec3 pixelNorm = normalize(vsNormal);
        vec3 diffuse = max(dot(pixelNorm, lightDir), 0.0) * lightColor;

        // 3. 스페큘러
        // vec3 reflectDir = -lightDir;
        // vec3 doubledProjectedNormal = pixelNorm * dot(pixelNorm, lightDir) * 2;
        // reflectDir = doubledNormal + reflectDir;
        vec3 viewDir = normalize(viewPos - vsPos);
        vec3 reflectDir = reflect(-lightDir, pixelNorm);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), specularShininess);
        vec3 specular = specularStrength * spec * lightColor;

        vec3 result = (ambient + diffuse + specular) * lightColor * objectColor;

        fragColor = vec4(result, 1.0);
        // fragColor = vec4(1.0, 1.0, 1.0, 1.0);
}
