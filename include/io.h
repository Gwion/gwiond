#ifndef IO_H
#define IO_H

//#define BUFFER_LENGTH 100
#define BUFFER_LENGTH 50

typedef struct {
  char *uri;
  char *content;
  Context context;
  CommentList *comments;
  struct Map_     docs;
} Buffer;

MK_VECTOR_TYPE(Buffer, buffer);

typedef struct Workspace {
  BufferList *buffers;  
  Gwion Gwion;
  // Lsp *lsp
} Workspace;


/*
 * Opens a new buffer.
 */
Buffer* open_buffer(MemPool mp, const char *uri, const char *content);

/*
 * Updates content of an existing buffer.
 */
Buffer* update_buffer(MemPool mp, const char *uri, const char *content);

/*
 * Searches a buffer by `uri` and returns its handle.
 */
Buffer get_buffer(const char *uri);

/*
 * Closes a buffer.
 */
void close_buffer(const Gwion, const char *uri);

/*
 * Truncates a given string to the specified length.
 */
void truncate_string(char *text, int line, int character);

/*
 * Returns the last symbol in a string.
 */
char *extract_last_symbol(char *text, bool *dot);

#endif /* end of include guard: IO_H */
