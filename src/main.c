#include "lsp.h"
#include <signal.h>
#include "gwion_util.h"
#include "gwion_ast.h"
#include "gwion_env.h"
#include "vm.h"
#include "gwion.h"
#include "arg.h"
#include "gwiond.h"

int main() {
  gwiond_ini();
  lsp_event_loop();
  gwiond_end();
  return 0;
}
