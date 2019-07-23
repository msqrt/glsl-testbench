
void lookAt(float* cameraToWorld, float camx, float camy, float camz, float atx, float aty, float atz, float upx = .0f, float upy = 1.f, float upz = .0f);
void setupProjection(float* projection, float fov, float w_over_h, float nearPlane, float farPlane);
void setupOrtho(float* ortho, float w_over_h, float size, float nearPlane = .1f, float farPlane = 20.f);

template<typename T>
void printBits(T t) {
	uint8_t* s = (uint8_t*)(&t);
	for(int i = sizeof(T)-1; i>=0; --i)
	for (int b = 7; b >= 0; --b)
		printf("%d", (s[i] >> b) & 1);
}
