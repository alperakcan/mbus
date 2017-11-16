
struct mbus_buffer;

struct mbus_buffer * mbus_buffer_create (void);
void mbus_buffer_destroy (struct mbus_buffer *buffer);
int mbus_buffer_reset (struct mbus_buffer *buffer);
unsigned int mbus_buffer_size (struct mbus_buffer *buffer);
unsigned int mbus_buffer_length (struct mbus_buffer *buffer);
int mbus_buffer_set_length (struct mbus_buffer *buffer, unsigned int length);
uint8_t * mbus_buffer_base (struct mbus_buffer *buffer);
int mbus_buffer_reserve (struct mbus_buffer *buffer, unsigned int length);
int mbus_buffer_push (struct mbus_buffer *buffer, const void *data, unsigned int length);
int mbus_buffer_push_string (struct mbus_buffer *buffer, enum mbus_compress_method compression, const char *string);
int mbus_buffer_shift (struct mbus_buffer *buffer, unsigned int length);
