#include <ctype.h>
#include "err_codes.h"
#include "gwion_util.h"
#include "gwion_ast.h"
#include "gwion_env.h"
#include "vm.h"
#include "gwion.h"
#include "io.h"

static Buffer buffers[BUFFER_LENGTH];
static unsigned int first_empty_buf;

Buffer* open_buffer(MemPool mp, const char *uri, const char *content) {
  if(first_empty_buf >= BUFFER_LENGTH)
    exit(EXIT_BUFFERS_FULL);
  Buffer *buffer = buffers + first_empty_buf++;
  buffer->uri = mstrdup(mp, uri);
  buffer->content = mstrdup(mp, content);
  buffer->comments = new_commentlist(mp, 0);
  map_init(&buffer->docs);
  return buffer;
}

static void free_docs(MemPool mp, Map docs) {
  for(m_uint i = 0; i < map_size(docs); i++) {
    GwText *text = (GwText*)map_at(docs, i);
    free_text(text);
  }
}

static void free_comments(const MemPool mp, CommentList *comments) {
  for(uint32_t i = 0; i < comments->len; i++) {
    Comment c = commentlist_at(comments, i);
    free_mstr(mp, c.str);
  }
}

Buffer* update_buffer(MemPool mp, const char *uri, const char *content) {
  for(unsigned int i = 0; i < first_empty_buf; i++) {
    if(strcmp(buffers[i].uri, uri) == 0) {
      free_mstr(mp, buffers[i].content);
      buffers[i].content = mstrdup(mp, content);
      if(buffers[i].comments) {
        free_comments(mp, buffers[i].comments);
        buffers[i].comments->len = 0;
      }
      if(buffers[i].docs.ptr) {
        free_docs(mp, &buffers[i].docs);
        map_clear(&buffers[i].docs);
      }
      buffers[i].context = NULL;
      return &buffers[i];
    }
  }
  exit(EXIT_BUFFER_NOT_OPEN);
}

Buffer get_buffer(const char *uri) {
  for(unsigned int i = 0; i < first_empty_buf; i++) {
    if(!strcmp(buffers[i].uri, uri)) {
      return buffers[i];
    }
  }
  exit(EXIT_BUFFER_NOT_OPEN);
}

void close_buffer(const Gwion gwion, const char *uri) {
  MemPool mp = gwion->mp;
  for(unsigned int i = 0; i < first_empty_buf; i++) {
    Buffer buffer = buffers[i];
    if(strcmp(buffer.uri, uri) == 0) {
      free_mstr(mp, buffer.uri);
      if(buffer.content)
        free_mstr(mp, buffer.content);
      if(buffers[i].comments) {
        free_comments(mp, buffers[i].comments);
        free_commentlist(mp, buffer.comments);
      }
      if(buffers[i].docs.ptr) {
        free_docs(mp, &buffers[i].docs);
        map_release(&buffers[i].docs);
      }
      if(buffer.context)
        free_context(buffer.context, gwion);

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
  while(isalnum(*(text + position)) || *(text+position) == '_')
    ++position;
  text[position] = '\0';
}

static bool isdot(const char* text, const size_t len, bool *dot) {
  for(unsigned int position = len; position > 0; position--) {
    if(text[position-1] == '.') {
      return true;
    }
    if(!isspace(text[position-1])) {
      break;
    }
  }
  return false;

}

char *extract_last_symbol(char *text, bool *dot) {
  for(unsigned int position = strlen(text) - 1; position > 0; position--) {
    if(!isalnum(text[position - 1])) {
      *dot = isdot(text, position, dot);
      return text + position;
    }
  }
  return text;
}
