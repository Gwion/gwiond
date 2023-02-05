#include "gwion_util.h"

Vector trie;
MemPool mp;

static Vector new_trie(int max) {
  const Vector v = new_vector(mp);
  for(int i = 0; i < max; i++)
    vector_add(v, 0);
  return v;
}

ANN void trie_ini(MemPool _mp) {
  mp = _mp;
  trie = new_trie(256);
}

ANN Vector trie_at(Vector vec, const uint32_t i) {
  return (Vector)vector_at(vec, i);
}

ANN void trie_clean(Vector vec) {
  Vector v = (Vector)vector_front(vec);
  if(v) free_vector(mp, v);
  for(uint32_t i = 0; i < vector_size(vec); i++) {
    Vector tmp = trie_at(vec, i);
    if(!tmp) continue;
    trie_clean(vec);
    free_vector(mp, vec);
  }
}
ANN void trie_end(void) {
  trie_clean(trie);
}

ANN static Vector trie_append(Vector vec, const char c, const int max) {
  Vector tmp = trie_at(vec, c);
  if(tmp) return tmp;
  Vector ret = new_trie(max); // need to assign it
  vector_set(vec, c, (vtype)ret);
  return ret;
}

ANN void trie_add(char *name, void *value) {
  Vector vec = trie;
  char c;
  while((c = *name++))
    vec = trie_append(vec, c, 256);
  vec = trie_append(vec, c, c);
  if(vector_find(vec, (vtype)value) == -1)
    vector_add(vec, (vtype)value);
}

ANN Vector trie_get(char *const name) {
  char *s = name;
  Vector vec = trie;
  char c;
  while((c = *s++))
    CHECK_OO((vec = trie_at(vec, c)));
  return trie_at(vec, c);
}

ANN static void get_values(const Vector ret, Vector vec) {
  Vector values = (Vector) vector_front(vec);
  if(values) {
    for(m_uint i = 0; i < vector_size(values); i++)
      vector_add(ret, vector_at(values, i));
  }
  for(m_uint i = 1; i < 256; i++) {
    Vector tmp = (Vector)vector_at(vec, i);
    if(tmp) get_values(ret, tmp);
  }
}

ANN Vector trie_starts_with(char *partial) {
  Vector vec = trie;
  char c;
  while((c = *partial++))
    CHECK_OO((vec = trie_at(vec, c)));
  Vector ret = new_vector(mp);
  get_values(ret, vec);
  return ret;
}

ANN static void _trie_clear(Vector vec) {
  Vector v = (Vector)vector_front(vec);
  if(v) vector_clear(v);
  for(uint32_t i = 1; i < vector_size(vec); i++) {
    Vector v = (Vector)vector_at(vec, i);
    if(v) _trie_clear(v);
  }
}

void trie_clear(void) {
  _trie_clear(trie);
}
