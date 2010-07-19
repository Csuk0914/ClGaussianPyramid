__kernel void
optionallydownscaledconvolution(
        __write_only image2d_t output_image, 
        int output_origin_x,
        int output_origin_y,
        __read_only image2d_t input_image,
        int input_origin_x,
        int input_origin_y,
        int downscaleOrNot)
{
    /*
    const float mask[5][5] = {
        0.003906, 0.015625, 0.023438, 0.015625, 0.003906, 
        0.015625, 0.062500, -0.093750, 0.062500, 0.015625, 
        0.023438, -0.093750, -0.140625, -0.093750, 0.023438, 
        0.015625, 0.062500, -0.093750, 0.062500, 0.015625, 
        0.003906, 0.015625, 0.023438, 0.015625, 0.003906
    };
/*/
    const float mask[5][5] = {
        0.003906, 0.015625, 0.023438, 0.015625, 0.003906, 
        0.015625, 0.062500, 0.093750, 0.062500, 0.015625, 
        0.023438, 0.093750, 0.140625, 0.093750, 0.023438, 
        0.015625, 0.062500, 0.093750, 0.062500, 0.015625, 
        0.003906, 0.015625, 0.023438, 0.015625, 0.003906
    };
    //*/
    const sampler_t sampler = 
        CLK_FILTER_NEAREST|CLK_NORMALIZED_COORDS_FALSE|CLK_ADDRESS_CLAMP_TO_EDGE;

    int sf = downscaleOrNot ? 2 : 1;

    int x_in_output = output_origin_x + get_global_id(0);
    int y_in_output = output_origin_y + get_global_id(1);

    int x_in_input = input_origin_x + get_global_id(0)*sf;
    int y_in_input = input_origin_y + get_global_id(1)*sf;

    float4 c = (float4)0.f;

    c += convert_float4(read_imageui(input_image, sampler, (int2)(x_in_input-2*sf, y_in_input-2*sf))) * mask[0][0];
    c += convert_float4(read_imageui(input_image, sampler, (int2)(x_in_input-1*sf, y_in_input-2*sf))) * mask[1][0];
    c += convert_float4(read_imageui(input_image, sampler, (int2)(x_in_input+0*sf, y_in_input-2*sf))) * mask[2][0];
    c += convert_float4(read_imageui(input_image, sampler, (int2)(x_in_input+1*sf, y_in_input-2*sf))) * mask[3][0];
    c += convert_float4(read_imageui(input_image, sampler, (int2)(x_in_input+2*sf, y_in_input-2*sf))) * mask[4][0];
    c += convert_float4(read_imageui(input_image, sampler, (int2)(x_in_input-2*sf, y_in_input-1*sf))) * mask[0][1];
    c += convert_float4(read_imageui(input_image, sampler, (int2)(x_in_input-1*sf, y_in_input-1*sf))) * mask[1][1];
    c += convert_float4(read_imageui(input_image, sampler, (int2)(x_in_input+0*sf, y_in_input-1*sf))) * mask[2][1];
    c += convert_float4(read_imageui(input_image, sampler, (int2)(x_in_input+1*sf, y_in_input-1*sf))) * mask[3][1];
    c += convert_float4(read_imageui(input_image, sampler, (int2)(x_in_input+2*sf, y_in_input-1*sf))) * mask[4][1];
    c += convert_float4(read_imageui(input_image, sampler, (int2)(x_in_input-2*sf, y_in_input+0*sf))) * mask[0][2];
    c += convert_float4(read_imageui(input_image, sampler, (int2)(x_in_input-1*sf, y_in_input+0*sf))) * mask[1][2];
    c += convert_float4(read_imageui(input_image, sampler, (int2)(x_in_input+0*sf, y_in_input+0*sf))) * mask[2][2];
    c += convert_float4(read_imageui(input_image, sampler, (int2)(x_in_input+1*sf, y_in_input+0*sf))) * mask[3][2];
    c += convert_float4(read_imageui(input_image, sampler, (int2)(x_in_input+2*sf, y_in_input+0*sf))) * mask[4][2];
    c += convert_float4(read_imageui(input_image, sampler, (int2)(x_in_input-2*sf, y_in_input+1*sf))) * mask[0][3];
    c += convert_float4(read_imageui(input_image, sampler, (int2)(x_in_input-1*sf, y_in_input+1*sf))) * mask[1][3];
    c += convert_float4(read_imageui(input_image, sampler, (int2)(x_in_input+0*sf, y_in_input+1*sf))) * mask[2][3];
    c += convert_float4(read_imageui(input_image, sampler, (int2)(x_in_input+1*sf, y_in_input+1*sf))) * mask[3][3];
    c += convert_float4(read_imageui(input_image, sampler, (int2)(x_in_input+2*sf, y_in_input+1*sf))) * mask[4][3];
    c += convert_float4(read_imageui(input_image, sampler, (int2)(x_in_input-2*sf, y_in_input+2*sf))) * mask[0][4];
    c += convert_float4(read_imageui(input_image, sampler, (int2)(x_in_input-1*sf, y_in_input+2*sf))) * mask[1][4];
    c += convert_float4(read_imageui(input_image, sampler, (int2)(x_in_input+0*sf, y_in_input+2*sf))) * mask[2][4];
    c += convert_float4(read_imageui(input_image, sampler, (int2)(x_in_input+1*sf, y_in_input+2*sf))) * mask[3][4];
    c += convert_float4(read_imageui(input_image, sampler, (int2)(x_in_input+2*sf, y_in_input+2*sf))) * mask[4][4];

    write_imageui(output_image, (int2)(x_in_output, y_in_output), convert_int4(c));
}

