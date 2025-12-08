#version 330 core

in vec3 FragPos;

uniform vec3 objectColor;
uniform vec3 lightPos;   // 포인트 라이트 위치

out vec4 FragColor;

void main()
{
    float dist = length(FragPos - lightPos);

    // 거리 기반 감쇠
    float attenuation = 1.0 / (1.0 + 0.4 * dist + 0.6 * dist * dist);

    // 약간의 주변광
    vec3 ambient = objectColor * 0.15;

    vec3 color = ambient + objectColor * attenuation;
    color = clamp(color, 0.0, 1.0);

    FragColor = vec4(color, 1.0);
}
