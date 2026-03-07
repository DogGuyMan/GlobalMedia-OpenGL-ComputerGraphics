#version 430 core
out vec4 color;

// 픽셀별로 뿌려질 픽셀값을 out이라는 키워드를 사용해서 전달하기로 했다.
void main(void) 
{
    color = vec4(0,0, 0.8, 1.0, 1.0);
}