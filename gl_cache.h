#define GL_CACHE_VERSION        1

typedef struct gl_cache_header_s
{
  int version;
  int crc;
  int length;
} gl_cache_header_t;

void GL_Cache_Init(void);
