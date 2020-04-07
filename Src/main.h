#include "stm32f1xx.h"
#include <stdlib.h>
#include "stdio.h"
#include "strings.h"
#include "rcc.h"
#include "sdio.h"
#include "ff.h"
#include "log.h"
#include "common_defs.h"

#define DATA_SIZE 16*1024
#define WRITE_COUNT 255


