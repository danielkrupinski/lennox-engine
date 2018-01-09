// Sta�y bufor, kt�ry przechowuje trzy podstawowe macierze z kolejno�ci� wed�ug kolumn do tworzenia geometrii.
cbuffer ModelViewProjectionConstantBuffer : register(b0)
{
	matrix model;
	matrix view;
	matrix projection;
};

// Dane ka�dego wierzcho�ka u�ywane jako dane wej�ciowe programu do cieniowania wierzcho�k�w.
struct VertexShaderInput
{
	float3 pos : POSITION;
	float3 color : COLOR0;
};

// Dane koloru ka�dego piksela przekazywane za po�rednictwem programu do cieniowania pikseli.
struct PixelShaderInput
{
	float4 pos : SV_POSITION;
	float3 color : COLOR0;
};

// Prosty program do cieniowania przetwarzaj�cy wierzcho�ki na procesorze GPU.
PixelShaderInput main(VertexShaderInput input)
{
	PixelShaderInput output;
	float4 pos = float4(input.pos, 1.0f);

	// Transformacja po�o�enia wierzcho�ka do rzutowanej przestrzeni.
	pos = mul(pos, model);
	pos = mul(pos, view);
	pos = mul(pos, projection);
	output.pos = pos;

	//Przeka� kolor bez modyfikacji.
	output.color = input.color;

	return output;
}
