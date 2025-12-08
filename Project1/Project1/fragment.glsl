#version 330 core

in vec3 FragPos;
in vec3 Normal;

uniform vec3 objectColor;
uniform vec3 lightPos;   // 포인트 라이트 위치
uniform vec3 lightDir;   // 스포트라이트 방향
uniform float cutOff;
uniform float outerCutOff;

out vec4 FragColor;

void main()
{
    vec3 norm = normalize(Normal);
    vec3 lightDirection = normalize(lightPos - FragPos);

    // Diffuse
    float diff = max(dot(norm, lightDirection), 0.0);
    vec3 diffuse = diff * objectColor;

    // Spotlight (soft edges)
    float theta = dot(lightDirection, normalize(-lightDir));
    float epsilon = cutOff - outerCutOff;
    float intensity = clamp((theta - outerCutOff) / epsilon, 0.0, 1.0);

    // Distance attenuation (기존 로직 유지)
    float dist = length(FragPos - lightPos);
    float attenuation = 1.0 / (1.0 + 0.4 * dist + 0.6 * dist * dist);

    vec3 ambient = objectColor * 0.15;
    vec3 color = ambient + diffuse * intensity;
    color *= attenuation;
    color = clamp(color, 0.0, 1.0);

    FragColor = vec4(color, 1.0);
}
