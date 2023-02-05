#include "gwion_util.h"
#include "gwiond.h"
#include "lsp.h"

int main() {
  gwiond_ini();
  lsp_event_loop();
  gwiond_end();
  return 0;
}
