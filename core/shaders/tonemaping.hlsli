// ACES tone mapping curve fit to go from HDR to LDR
//https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
float3 ACESFilm(float3 x) {
   float a = 2.51f;
   float b = 0.03f;
   float c = 2.43f;
   float d = 0.59f;
   float e = 0.14f;
   return saturate((x*(a*x+b))/(x*(c*x+d)+e));
}

float3 GammaCorrection(float3 x) {
   return pow(x, 1 / 2.2);
}