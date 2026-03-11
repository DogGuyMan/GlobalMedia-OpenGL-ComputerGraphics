#version 410 core

// 출력으로서 GL 포지션으로 3차원 좌표를 보내게 되고 프래그먼트 쉐이더가  
void main(void)
{
    gl_Position = vec4(0.0, 0.0, 0.5, 1.0);
}