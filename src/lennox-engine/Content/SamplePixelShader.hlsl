// Dane koloru ka�dego piksela przekazywane za po�rednictwem programu do cieniowania pikseli.
struct PixelShaderInput
{
	float4 pos : SV_POSITION;
	float3 color : COLOR0;
};

// Funkcja przekazuj�ca dla danych koloru (interpolowanego).
float4 main(PixelShaderInput input) : SV_TARGET
{
	return float4(input.color, 1.0f);
}
