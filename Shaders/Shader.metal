#include <metal_stdlib>
using namespace metal;

struct v2f
{
    float4 position [[position]];
    half3 color;
};

struct VertexData
{
    device float3* positions [[id(0)]];
    device float3* colors [[id(1)]];
};

struct FrameData
{
    float angle;
    int current_range;
};

v2f vertex vertexMain( device const VertexData* vertexData [[buffer(0)]], constant FrameData* frameData [[buffer(1)]], uint vertexId [[vertex_id]] )
{
    float angle = frameData->angle;
    int current_range = frameData->current_range;
    float3x3 rotationMatrix = float3x3( -sin(angle), -cos(angle), 0.0, cos(angle), -sin(angle), 0.0, 0.0, 0.0, 1.0 );
    v2f o;
    o.position = float4( rotationMatrix * vertexData->positions[ vertexId ], 1.0 );
    
    half3 color;
    //(from 0 to 6π) π/2
    float pi_values[13] = {0.0, 1.57, 3.14,
        4.71, 6.28, 7.85,
        9.42, 11.00, 12.57,
        14.14,15.71,17.28,18.85};

        //starting red
        float possible_values[12] = {cos(angle),0,-sin(angle),-sin(angle),
                                     0,-cos(angle),-cos(angle),0,
                                     sin(angle),sin(angle),0,cos(angle)};

        int v1_color_red_index = current_range;

        int v1_color_green_index = current_range + 8;

        int v1_color_blue_index = current_range +4;


        if(v1_color_green_index > 12)
        {
            v1_color_green_index-=12;
        }

         if(v1_color_blue_index > 12)
        {
            v1_color_blue_index-=12;
        }

        if(vertexId ==0)
        {
        half3 v1_color = half3(possible_values[v1_color_red_index],
                               possible_values[v1_color_green_index],
                               possible_values[v1_color_blue_index]);
            o.color = v1_color;
        }  
        else if(vertexId == 1)
        {            
        half3 v2_color = half3(possible_values[v1_color_blue_index],
                               possible_values[v1_color_red_index],
                               possible_values[v1_color_green_index]);
            o.color = v2_color;
        }
        else
        {
        half3 v3_color = half3(possible_values[v1_color_green_index],
                                         possible_values[v1_color_blue_index],
                                         possible_values[v1_color_red_index]);
            o.color = v3_color;
        }
          
        return o;
    
    o.color = color;
    
    return o;
}

half4 fragment fragmentMain( v2f in [[stage_in]] )
{
    
    return half4(in.color, 1.0 );
}
