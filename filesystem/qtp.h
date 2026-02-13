// qtp.h
#ifndef QTP_H
#define QTP_H

#ifdef __cplusplus
extern "C" {
#endif

	// Загрузить текстуру по имени из QTP-пака.
	// expectedWidth/Height можно использовать для проверки или игнорировать.
	// Возвращает gl_texturenum или 0 при ошибке.
	int QTP_LoadTexture(const char* name, int expectedWidth, int expectedHeight);

#ifdef __cplusplus
}
#endif

#endif // QTP_H
