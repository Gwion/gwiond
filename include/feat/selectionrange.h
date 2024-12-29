typedef struct SelectionRange SelectionRange;
struct SelectionRange {
  ValueList *values; // move me
  // could also needa be loclist
  // and hold a value/trait to match
  // so make it some
  /** pointer for passes to add and match stuff **/
  void *data;
  pos_t pos;
  bool (*addv)(SelectionRange *, Value);
  bool (*inside)(SelectionRange *, const loc_t);
  uint32_t scope; // check size in gwion
};
