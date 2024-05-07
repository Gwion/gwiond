#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "err_codes.h"
#include "gwion_util.h"
#include "gwion_ast.h"
#include "gwion_env.h"
#include "vm.h"
#include "gwion.h"
#include "io.h"

static Buffer buffers[BUFFER_LENGTH];
unsigned int first_empty_buf;

Buffer* open_buffer(MemPool mp, const char *uri, const char *content) {
  if(first_empty_buf >= BUFFER_LENGTH)
    exit(EXIT_BUFFERS_FULL);
  buffers[first_empty_buf].uri = mstrdup(mp, uri);
  buffers[first_empty_buf].content = mstrdup(mp, content);
  return &buffers[first_empty_buf++];
}

Buffer* update_buffer(MemPool mp, const char *uri, const char *content) {
  for(unsigned int i = 0; i < first_empty_buf; i++) {
    if(strcmp(buffers[i].uri, uri) == 0) {
      free_mstr(mp, buffers[i].content);
      buffers[i].content = mstrdup(mp, content);
      return &buffers[i];
    }
  }
  exit(EXIT_BUFFER_NOT_OPEN);
}

Buffer get_buffer(const char *uri) {
  for(unsigned int i = 0; i < first_empty_buf; i++) {
    if(strcmp(buffers[i].uri, uri) == 0) {
      return buffers[i];
    }
  }
  exit(EXIT_BUFFER_NOT_OPEN);
}

void close_buffer(const Gwion gwion, const char *uri) {
  for(unsigned int i = 0; i < first_empty_buf; i++) {
    Buffer buffer = buffers[i];
    if(strcmp(buffer.uri, uri) == 0) {
      free_mstr(gwion->mp, buffer.uri);
      free_mstr(gwion->mp, buffer.content);
      if(buffer.context) free_context(buffer.context, gwion);
      for(unsigned int j = i; j < first_empty_buf - 1; j++) {
        buffers[j] = buffers[j + 1];
      }
      --first_empty_buf;
      return;
    }
  }
  exit(EXIT_BUFFER_NOT_OPEN);
}

void truncate_string(char *text, int line, int character) {
  unsigned int position = 0;
  for(int i = 0; i < line; i++)
    position += strcspn(text + position, "\n") + 1;
  position += character;
  if(position >= strlen(text))
    return;
  while(isalnum(*(text + position)))
    ++position;
  text[position] = '\0';
}

char *extract_last_symbol(char *text) {
  for(unsigned int position = strlen(text) - 1; position > 0; position--) {
    if(!isalnum(text[position - 1]))
      return text + position;
  }
  return text;
}
