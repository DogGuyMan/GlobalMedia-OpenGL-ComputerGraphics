#version 430 core

in vec3 vsNormal;  
in vec3 vsPos;  
in vec2 vsTexCoord;

struct Material { 
    sampler2D diffuse; 
    sampler2D specular; 
    float shininess; 

	vec3 defaultAmbient;
	vec3 defaultDiffuse;
	vec3 defaultSpecular;
	int useDiffuseMap;
	int useSpecularMap;
}; 

struct Light { 
    vec3 position; 

    vec3 ambient; 
    vec3 diffuse; 
    vec3 specular; 
}; 

uniform Material material;
uniform vec3 viewPos; 
uniform Light light; 

uniform int useNormal;

out vec4 fragColor;

vec3 matAmbientColor;
vec3 matDiffuseColor;
vec3 matSpecularColor;

void main()
{
	if(useNormal != 0)
	{
		// properties
		vec3 norm = normalize(vsNormal);
		vec3 viewDir = normalize(viewPos - vsPos);

		matAmbientColor = material.defaultAmbient;
		matDiffuseColor = material.defaultDiffuse;
		matSpecularColor = material.defaultSpecular;

		if(material.useDiffuseMap != 0)
		{
			matAmbientColor = vec3(texture(material.diffuse, vsTexCoord));
			matDiffuseColor = vec3(texture(material.diffuse, vsTexCoord));
		}

		if(material.useSpecularMap != 0)
		{
			matSpecularColor = vec3(texture(material.specular, vsTexCoord));
		}
    
		// ambient
		vec3 ambient = light.ambient * matAmbientColor;
  	
		// diffuse 
		vec3 lightDir = normalize(light.position - vsPos);
		float diff = max(dot(norm, lightDir), 0.0);
		vec3 diffuse = light.diffuse * diff * matDiffuseColor;
    
		// specular
		vec3 reflectDir = reflect(-lightDir, norm);  
		float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
		vec3 specular = light.specular * spec * matSpecularColor;  
        
		vec3 result = ambient + diffuse + specular;
		fragColor = vec4(result, 1.0);
	}
	else
		fragColor = vec4(material.defaultDiffuse, 1.0);
} 
