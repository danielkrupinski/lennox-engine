// Sta³y bufor, który przechowuje trzy podstawowe macierze z kolejnoœci¹ wed³ug kolumn do tworzenia geometrii.
cbuffer ModelViewProjectionConstantBuffer : register(b0)
{
	matrix model;
	matrix view;
	matrix projection;
};

// Dane ka¿dego wierzcho³ka u¿ywane jako dane wejœciowe programu do cieniowania wierzcho³ków.
struct VertexShaderInput
{
	float3 pos : POSITION;
	float3 color : COLOR0;
};

// Dane koloru ka¿dego piksela przekazywane za poœrednictwem programu do cieniowania pikseli.
struct PixelShaderInput
{
	float4 pos : SV_POSITION;
	float3 color : COLOR0;
};

// Prosty program do cieniowania przetwarzaj¹cy wierzcho³ki na procesorze GPU.
PixelShaderInput main(VertexShaderInput input)
{
	PixelShaderInput output;
	float4 pos = float4(input.pos, 1.0f);

	// Transformacja po³o¿enia wierzcho³ka do rzutowanej przestrzeni.
	pos = mul(pos, model);
	pos = mul(pos, view);
	pos = mul(pos, projection);
	output.pos = pos;

	//Przeka¿ kolor bez modyfikacji.
	output.color = input.color;

	return output;
}
