// #version 430 core
#version 410 core

in vec3 vsPosition;
in vec3 vsColor;
in vec3 vsNormal;
in vec2 vsTexCoord;

struct Light {
        vec3 position;
        vec3 ambient;
        vec3 diffuse;
        vec3 specular;
};
uniform Light light;

struct Material {
        sampler2D diffuse;
        sampler2D specular;
};
uniform Material material;

uniform vec3 viewPos;
uniform vec3 objectColor;

uniform float useTexture;

out vec4 fragColor;

void main()
{
        if (useTexture < 0.5) {
                fragColor = vec4(objectColor, 1.0);
                return;
        }

        // ambient
        vec3 diffuseTextureColor = texture(material.diffuse, vsTexCoord).rgb;
        vec3 ambient = diffuseTextureColor * light.ambient;

        // diffuse
        vec3 lightDir = normalize(light.position - vsPosition);
        vec3 pixelNorm = normalize(vsNormal);

        float diff = max(dot(pixelNorm, lightDir), 0.0);
        vec3 diffuse = diff * diffuseTextureColor * light.diffuse;

        // specular
        vec3 specTextureColor = texture(material.specular, vsTexCoord).rgb;
        vec3 viewDir = normalize(viewPos - vsPosition);
        vec3 reflectDir = reflect(-lightDir, pixelNorm);
        int shininess = 32;
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
        vec3 specular = spec * specTextureColor * light.specular;

        vec3 result = (ambient + diffuse + specular) * objectColor;
        fragColor = vec4(result, 1.0);
}
