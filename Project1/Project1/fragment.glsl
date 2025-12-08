#version 330 core

in vec3 FragPos;
in vec3 Normal;

out vec4 FragColor;

uniform vec3 objectColor;
uniform vec3 lightPos;

void main()
{
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);

    // Ambient (기본 밝기)
    vec3 ambient = objectColor * 0.10;

    // Diffuse
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = objectColor * diff * 3.5;  // 밝기를 높임

    // Distance attenuation (최소한 적용)
    float dist = length(lightPos - FragPos);
    float attenuation = 1.0 / (1.0 + 0.5 * dist + 0.01 * dist * dist);

    vec3 result = (ambient + diffuse) * attenuation;
    FragColor = vec4(result, 1.0);
}
