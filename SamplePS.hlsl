struct VSOutput{
    float4 svpos : SV_POSITION;
    float4 color : COLOR;
};

float4 pixel(VSOutput input) : SV_TARGET
{
    return input.color;
}