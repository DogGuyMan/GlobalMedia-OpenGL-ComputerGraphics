#version 430 core

in vec3 vsNormal;  
in vec3 vsPos;  
  
uniform vec3 lightPos; 
uniform vec3 viewPos; 
uniform vec3 lightColor;
uniform vec3 objectColor;

out vec4 fragColor;

void main()
{
    // ambient
    float ambientStrength = 0.1;
    vec3 ambient = ambientStrength * lightColor;
  	
    // diffuse 
    vec3 norm = normalize(vsNormal);
    vec3 lightDir = normalize(lightPos - vsPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;
    
    // specular
    float specularStrength = 1.0;
    vec3 viewDir = normalize(viewPos - vsPos);
    vec3 reflectDir = reflect(-lightDir, norm);  
    int shininess = 32;
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), shininess);
    vec3 specular = specularStrength * spec * lightColor;  
        
    vec3 result = (ambient + diffuse + specular) * objectColor;
    fragColor = vec4(result, 1.0);
} 