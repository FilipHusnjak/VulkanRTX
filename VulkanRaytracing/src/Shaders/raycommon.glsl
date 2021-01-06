struct hitPayload
{
    int depth;
	vec3 hitValue;
    vec3 attenuation;
};

float fresnelSchlick(float cosTheta, float F0)
{
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}
