#include "config.h"
#include <stdio.h>

int main(int argc, char *argv[]) {
  (void)argc;
  (void)argv;

  Config conf;
  parse_config((argc > 1) ? argv[1] : NULL, &conf);

  return 0;
}
