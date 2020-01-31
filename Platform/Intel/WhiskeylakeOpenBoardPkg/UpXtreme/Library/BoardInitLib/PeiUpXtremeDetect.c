/** @file

  Copyright (c) 2020, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <PiPei.h>
#include <SaPolicyCommon.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/IoLib.h>
#include <Library/HobLib.h>
#include <Library/PcdLib.h>
#include <Library/PchCycleDecodingLib.h>
#include <Library/PciLib.h>
#include <Library/PcdLib.h>
#include <Library/BaseMemoryLib.h>

#include <Library/PeiSaPolicyLib.h>
#include <Library/BoardInitLib.h>
#include <PchAccess.h>
#include <Library/GpioNativeLib.h>
#include <Library/GpioLib.h>
#include <GpioPinsSklLp.h>
#include <GpioPinsSklH.h>
#include <Library/GpioExpanderLib.h>
#include <SioRegs.h>
#include <Library/PchPcrLib.h>

#include "UpXtremeInit.h"

#include <ConfigBlock.h>
#include <ConfigBlock/MemoryConfig.h>

EFI_STATUS
EFIAPI
UpXtremeBoardDetect (
  VOID
  )
{
  if (LibPcdGetSku () != 0) {
    return EFI_SUCCESS;
  }

  DEBUG ((DEBUG_INFO, "UpXtremeDetectionCallback\n"));

  LibPcdSetSku (BoardIdUpXtreme);

  DEBUG ((DEBUG_INFO, "SKU_ID: 0x%x\n", LibPcdGetSku ()));
  ASSERT (LibPcdGetSku () == BoardIdUpXtreme);

  return EFI_SUCCESS;
}

