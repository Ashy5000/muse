#include <string.h>

void *memcpy(void *restrict s1, const void *restrict s2, size_t n) {
	for (size_t i = 0; i < n; i++) {
		((unsigned char*)s1)[i] = ((unsigned char*)s2)[i];
	}
	return s1;
}

void *memmove(void *s1, const void *s2, size_t n) {
	if (s1 <= s2) {
		return memcpy(s1, s2, n);
	}
	for (size_t i = 0; i < n; i++) {
		((unsigned char*)s1)[n - i] = ((unsigned char*)s2)[n - i];
	}
	return s1;
}

char *strcpy(char *restrict s1, const char *restrict s2) {
	for (size_t i = 0; ; i++) {
		s1[i] = s2[i];
		if (!s2[i]) {
			break;
		}
	}
	return s1;
}

char *strncpy(char *restrict s1, const char *restrict s2, size_t n) {
	size_t i = 0;
	for (; i < n && s2[i]; i++) {
		s1[i] = s2[i];
	}
	for (; i < n; i++) {
		s1[i] = 0;
	}
	return s1;
}

char *strcat(char *restrict s1, const char *restrict s2) {
	size_t null_idx = 0;
	while (s1[null_idx]) { null_idx++; }
	size_t i = 0;
	while (s2[i]) {
		s1[null_idx + i] = s2[i];
		i++;
	}
	s1[null_idx + i] = 0;
	return s1;
}

char *strncat(char *restrict s1, const char *restrict s2, size_t n) {
	size_t null_idx = 0;
	while (s1[null_idx]) { null_idx++; }
	size_t i = 0;
	for(; i < n; i++) {
		if (!s2[i]) {
			i++;
			break;
		}
		s1[null_idx + i] = s2[i];
	}
	s1[null_idx + i] = 0;
	return s1;
}

int memcmp(const void *s1, const void *s2, size_t n) {
	for (size_t i = 0; i < n; i++) {
		unsigned char c1 = ((unsigned char*)s1)[i];
		unsigned char c2 = ((unsigned char*)s2)[i];
		if (c1 != c2) {
			return c1 - c2;
		}
	}
	return 0;
}

int strcmp(const char *s1, const char *s2) {
	size_t i = 0;
	while (s1[i] == s2[i]) {
		if (!s1[i]) {
			return 0;
		}
	}
	return ((unsigned char*)s1)[i] - ((unsigned char*)s2)[i];
}

// TODO: strcoll()

int strncmp(const char *s1, const char *s2, size_t n) {
	for (size_t i = 0; i < n; i++) {
		unsigned char c1 = ((unsigned char*)s1)[i];
		unsigned char c2 = ((unsigned char*)s2)[i];
		if (c1 != c2) {
			return c1 - c2;
		}
		if (c1 == 0) {
			break;
		}
	}
	return 0;
}

// TODO: strxfrm()

void *memchr(const void *s, int c, size_t n) {
	for (size_t i = 0; i < n; i++) {
		if (((unsigned char*)s)[i] == c) {
			return (void*)s + i;
		}
	}
	return 0;
}

char *strchr(const char *s, int c) {
	while (*s) {
		if (*s == c) {
			return (char*)s;
		}
		s++;
	}
	if (c == 0) {
		return (char*)s;
	}
	return 0;
}

size_t strcspn(const char *s1, const char *s2) {
	size_t i = 0;
	while (s1[i]) {
		size_t j = 0;
		while (s2[j]) {
			if (s1[i] == s2[j]) {
				return i;
			}
			j++;
		}
		i++;
	}
	return i;
}

char *strpbrk(const char *s1, const char *s2) {
	while (*s1) {
		size_t i = 0;
		while (s2[i]) {
			if (*s1 == s2[i]) {
				return (char*)s1;
			}
			i++;
		}
		s1++;
	}
	return 0;
}

char *strrchr(const char *s, int c) {
	char *res = 0;
	while (*s) {
		if (*s == c) {
			res = (char*)s;
		}
		s++;
	}
	return res;
}


size_t strspn(const char *s1, const char *s2) {
	unsigned char good = 0;
	size_t i = 0;
	while (*s1) {
		size_t j = 0;
		while (s2[j]) {
			if (s1[i] == s2[j]) {
				good = 1;
				break;
			}
			j++;
		}
		if (!good) {
			break;
		}
		good = 0;
		i++;
	}
	return i;
}
