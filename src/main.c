#include "gwion_util.h"
#include "gwion_ast.h"
#include "gwion_env.h"
#include "io.h"
#include "gwiond.h"
#include "lsp.h"

int main() {
  const Gwion gwion = gwiond_ini();
  lsp_event_loop(gwion);
  gwiond_end();
  return 0;
}
