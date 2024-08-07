Texture2D g_tex2D_Static;
Texture2D g_tex2D_StaticArr_0;
Texture2D g_tex2D_StaticArr_1;
Texture2D g_tex2D_Mut;
Texture2D g_tex2D_MutArr_0;
Texture2D g_tex2D_MutArr_1;
Texture2D g_tex2D_Dyn;
Texture2D g_tex2D_DynArr_0;
Texture2D g_tex2D_DynArr_1;

SamplerState g_tex2D_Static_sampler;
SamplerState g_tex2D_StaticArr_sampler;
SamplerState g_tex2D_Mut_sampler;
SamplerState g_tex2D_MutArr_sampler;
SamplerState g_tex2D_Dyn_sampler;
SamplerState g_tex2D_DynArr_sampler;

cbuffer UniformBuff_Stat
{
    float4 g_f4Color0;
}

cbuffer UniformBuff_Stat2
{
    float4 g_f4Color01;
}

cbuffer UniformBuff_Mut
{
    float4 g_f4Color1;
}

cbuffer UniformBuff_Dyn
{
    float4 g_f4Color2;
}

struct VSOut
{
    float4 f4Position : SV_Position;
    float4 f4Color	: COLOR;
};


VSOut main()
{
    VSOut Out;
    Out.f4Position = float4(0.0, 0.0, 0.0, 0.0);
    Out.f4Color = float4(0.0, 0.0, 0.0, 0.0);
    Out.f4Color += g_tex2D_Static.SampleLevel(g_tex2D_Static_sampler, float2(0.5,0.5), 0.0);
    Out.f4Color += g_tex2D_StaticArr_0.SampleLevel(g_tex2D_StaticArr_sampler, float2(0.5,0.5), 0.0) + g_tex2D_StaticArr_1.SampleLevel(g_tex2D_StaticArr_sampler, float2(0.5,0.5), 0.0);
    Out.f4Color += g_tex2D_Mut.SampleLevel(g_tex2D_Mut_sampler, float2(0.5,0.5), 0.0);
    Out.f4Color += g_tex2D_MutArr_0.SampleLevel(g_tex2D_MutArr_sampler, float2(0.5,0.5), 0.0) + g_tex2D_MutArr_1.SampleLevel(g_tex2D_MutArr_sampler, float2(0.5,0.5), 0.0);
    Out.f4Color += g_tex2D_Dyn.SampleLevel(g_tex2D_Dyn_sampler, float2(0.5,0.5), 0.0);
    Out.f4Color += g_tex2D_DynArr_0.SampleLevel(g_tex2D_DynArr_sampler, float2(0.5,0.5), 0.0) + g_tex2D_DynArr_1.SampleLevel(g_tex2D_DynArr_sampler, float2(0.5,0.5), 0.0);
    Out.f4Color += g_f4Color0 + g_f4Color01 + g_f4Color1 + g_f4Color2;

    return Out;
}
