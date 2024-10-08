/** @file

  Copyright (c) 2004  - 2020, Intel Corporation. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

Module Name:

  PlatformEarlyInit.c

Abstract:

  Do platform specific PEI stage initializations.

--*/


#include "PlatformEarlyInit.h"

static EFI_PEI_STALL_PPI  mStallPpi = {
  PEI_STALL_RESOLUTION,
  Stall
};

static EFI_PEI_PPI_DESCRIPTOR mInstallStallPpi = {
  EFI_PEI_PPI_DESCRIPTOR_PPI | EFI_PEI_PPI_DESCRIPTOR_TERMINATE_LIST,
  &gEfiPeiStallPpiGuid,
  &mStallPpi
};

//
// The reserved SMBus addresses are defined in PlatformDxe.h file.
//
static UINT8 mSmbusRsvdAddresses[] = PLATFORM_SMBUS_RSVD_ADDRESSES;
static PEI_SMBUS_POLICY_PPI         mSmbusPolicyPpi = {
  SMBUS_BASE_ADDRESS,
  SMBUS_BUS_DEV_FUNC,
  PLATFORM_NUM_SMBUS_RSVD_ADDRESSES,
  mSmbusRsvdAddresses
};

static EFI_PEI_PPI_DESCRIPTOR       mInstallSmbusPolicyPpi = {
  EFI_PEI_PPI_DESCRIPTOR_PPI | EFI_PEI_PPI_DESCRIPTOR_TERMINATE_LIST,
  &gPeiSmbusPolicyPpiGuid,
  &mSmbusPolicyPpi
};
static PEI_SPEAKER_IF_PPI    mSpeakerInterfacePpi = {
  ProgramToneFrequency,
  GenerateBeepTone
};

static EFI_PEI_PPI_DESCRIPTOR       mInstallSpeakerInterfacePpi = {
  EFI_PEI_PPI_DESCRIPTOR_PPI | EFI_PEI_PPI_DESCRIPTOR_TERMINATE_LIST,
  &gPeiSpeakerInterfacePpiGuid,
  &mSpeakerInterfacePpi
};

static EFI_PEI_RESET_PPI            mResetPpi = { IchReset };


static EFI_PEI_PPI_DESCRIPTOR       mPpiList[] = {
  {
    EFI_PEI_PPI_DESCRIPTOR_PPI,
    &gEfiPeiMasterBootModePpiGuid,
    NULL
  },
  {
    (EFI_PEI_PPI_DESCRIPTOR_PPI | EFI_PEI_PPI_DESCRIPTOR_TERMINATE_LIST),
    &gEfiPeiResetPpiGuid,
    &mResetPpi
  }
};

static EFI_PEI_NOTIFY_DESCRIPTOR    mNotifyList[] = {
  {
    EFI_PEI_PPI_DESCRIPTOR_NOTIFY_CALLBACK,
    &gEfiEndOfPeiSignalPpiGuid,
    (EFI_PEIM_NOTIFY_ENTRY_POINT)EndOfPeiPpiNotifyCallback
  },
  {
    (EFI_PEI_PPI_DESCRIPTOR_NOTIFY_CALLBACK| EFI_PEI_PPI_DESCRIPTOR_TERMINATE_LIST),
    &gEfiPeiMemoryDiscoveredPpiGuid,
    (EFI_PEIM_NOTIFY_ENTRY_POINT)MemoryDiscoveredPpiNotifyCallback
  }

};


/**

  Parse the status registers for figuring out the wake-up event and save it into
  an GUID HOB which will be referenced later. However, modification is required
  to meet the chipset register definition and the practical hardware design. Thus,
  this is just an example.


  @param PeiServices    pointer to the PEI Service Table
  @param EFI_SUCCESS    Always return Success

  @retval None


**/
EFI_STATUS
EFIAPI
GetWakeupEventAndSaveToHob (
  IN CONST EFI_PEI_SERVICES   **PeiServices
  )
{
  UINT16  Pm1Sts;
  UINTN   Gpe0Sts;
  UINTN   WakeEventData;

  //
  // Read the ACPI registers
  //
  Pm1Sts  = IoRead16 (ACPI_BASE_ADDRESS + R_PCH_ACPI_PM1_STS);
  Gpe0Sts = IoRead32 (ACPI_BASE_ADDRESS + R_PCH_ACPI_GPE0a_STS);

  //
  // Figure out the wake-up event
  //
  if ((Pm1Sts & B_PCH_ACPI_PM1_STS_PWRBTN) != 0) {
    WakeEventData = SMBIOS_WAKEUP_TYPE_POWER_SWITCH;
  } else if (((Pm1Sts & B_PCH_ACPI_PM1_STS_WAK) != 0)) {
    WakeEventData = SMBIOS_WAKEUP_TYPE_PCI_PME;
  } else if (Gpe0Sts != 0) {
    WakeEventData = SMBIOS_WAKEUP_TYPE_OTHERS;
  } else {
    WakeEventData = SMBIOS_WAKEUP_TYPE_UNKNOWN;
  }

  DEBUG ((DEBUG_ERROR, "ACPI Wake Status Register: %04x\n", Pm1Sts));
  DEBUG ((DEBUG_ERROR, "ACPI Wake Event Data: %02x\n", WakeEventData));

  return EFI_SUCCESS;
}

EFI_STATUS
GetSetupVariable (
  IN CONST EFI_PEI_SERVICES                **PeiServices,
  IN   SYSTEM_CONFIGURATION          *SystemConfiguration
  )
{
  UINTN                        VariableSize;
  EFI_STATUS                   Status;
  EFI_PEI_READ_ONLY_VARIABLE2_PPI   *Variable;

  VariableSize = sizeof (SYSTEM_CONFIGURATION);
  ZeroMem (SystemConfiguration, sizeof (SYSTEM_CONFIGURATION));

  Status = (*PeiServices)->LocatePpi (
                             PeiServices,
                             &gEfiPeiReadOnlyVariable2PpiGuid,
                             0,
                             NULL,
                                      (void **)&Variable
                             );
  ASSERT_EFI_ERROR (Status);

  //
  // Use normal setup default from NVRAM variable,
  // the Platform Mode (manufacturing/safe/normal) is handle in PeiGetVariable.
  //
  VariableSize = sizeof(SYSTEM_CONFIGURATION);
  Status = Variable->GetVariable (
                       Variable,
                       L"Setup",
                       &gEfiSetupVariableGuid,
                       NULL,
                       &VariableSize,
                       SystemConfiguration
                       );
  if (EFI_ERROR (Status)) {
    return Status;
  }
  if (VariableSize != sizeof(SYSTEM_CONFIGURATION)) {
    //
    // The setup variable is corrupted
    //
    return EFI_NOT_FOUND;
  }
  return EFI_SUCCESS;
}

EFI_STATUS
VlvPolicyInit (
  IN CONST EFI_PEI_SERVICES             **PeiServices,
  IN SYSTEM_CONFIGURATION         *SystemConfiguration
  )
{
  EFI_STATUS                      Status;
  EFI_PEI_PPI_DESCRIPTOR          *mVlvPolicyPpiDesc;
  VLV_POLICY_PPI                   *mVlvPolicyPpi;

  Status = (*PeiServices)->AllocatePool(
                             PeiServices,
                             sizeof (EFI_PEI_PPI_DESCRIPTOR),
                             (void **)&mVlvPolicyPpiDesc
                             );
  ASSERT_EFI_ERROR (Status);

  Status = (*PeiServices)->AllocatePool(
                             PeiServices,
                             sizeof (VLV_POLICY_PPI),
                             (void **)&mVlvPolicyPpi
                             );
  ASSERT_EFI_ERROR (Status);

  //
  // Initialize PPI
  //
  (*PeiServices)->SetMem ((VOID *)mVlvPolicyPpi, sizeof (VLV_POLICY_PPI), 0);
  mVlvPolicyPpiDesc->Flags = EFI_PEI_PPI_DESCRIPTOR_PPI | EFI_PEI_PPI_DESCRIPTOR_TERMINATE_LIST;
  mVlvPolicyPpiDesc->Guid = &gVlvPolicyPpiGuid;
  mVlvPolicyPpiDesc->Ppi = mVlvPolicyPpi;
  mVlvPolicyPpi->GtConfig.PrimaryDisplay = SystemConfiguration->PrimaryVideoAdaptor;
  mVlvPolicyPpi->GtConfig.IgdDvmt50PreAlloc = SystemConfiguration->IgdDvmt50PreAlloc;
  mVlvPolicyPpi->GtConfig.ApertureSize = SystemConfiguration->IgdApertureSize;
  mVlvPolicyPpi->GtConfig.GttSize = SystemConfiguration->GTTSize;
  if (SystemConfiguration->PrimaryVideoAdaptor != 2) {
    mVlvPolicyPpi->GtConfig.InternalGraphics = SystemConfiguration->Igd;
  } else {
    mVlvPolicyPpi->GtConfig.InternalGraphics = 0;
  }


  mVlvPolicyPpi->GtConfig.IgdTurboEn = 1;


  mVlvPolicyPpi->PlatformData.FastBoot = SystemConfiguration->FastBoot;
  mVlvPolicyPpi->PlatformData.DynSR = 1;
  DEBUG ((DEBUG_ERROR, "Setup Option ISPEn: 0x%x\n", SystemConfiguration->ISPEn));
  mVlvPolicyPpi->ISPEn                      = SystemConfiguration->ISPEn;
  DEBUG ((DEBUG_ERROR, "Setup Option ISPDevSel: 0x%x\n", SystemConfiguration->ISPDevSel));
  mVlvPolicyPpi->ISPPciDevConfig            = SystemConfiguration->ISPDevSel;
  if (SystemConfiguration->ISPEn == 0) {
    mVlvPolicyPpi->ISPPciDevConfig          = 0;
    DEBUG ((DEBUG_ERROR, "Update Setup Option ISPDevSel: 0x%x\n", mVlvPolicyPpi->ISPPciDevConfig));
  }
  Status = (*PeiServices)->InstallPpi(
                             PeiServices,
                             mVlvPolicyPpiDesc
                             );
  ASSERT_EFI_ERROR (Status);

  return EFI_SUCCESS;
}


EFI_STATUS
ConfigureSoCGpio (
  IN SYSTEM_CONFIGURATION  *SystemConfiguration
  )
{

    DEBUG ((DEBUG_ERROR, "ConfigureSoCGpio------------start\n"));
    if (SystemConfiguration->eMMCBootMode== 1) {// Auto detection mode
     DEBUG ((DEBUG_ERROR, "Auto detection mode------------start\n"));

     //
     //Silicon Steppings
     //
     switch (PchStepping()) {
       case PchA0:  // SOC A0 and A1
       case PchA1:
         DEBUG ((DEBUG_ERROR, "SOC A0/A1: eMMC 4.41 GPIO Configuration\n"));
         SystemConfiguration->LpsseMMCEnabled            = 1;
         SystemConfiguration->LpsseMMC45Enabled          = 0;
         break;
       case PchB0:  // SOC B0 and later
       default:
         DEBUG ((DEBUG_ERROR, "SOC B0 and later: eMMC 4.5 GPIO Configuration\n"));
         SystemConfiguration->LpsseMMCEnabled            = 0;
         SystemConfiguration->LpsseMMC45Enabled          = 1;
         break;
     }
    } else if (SystemConfiguration->eMMCBootMode == 2) { // eMMC 4.41
        DEBUG ((DEBUG_ERROR, "Force to eMMC 4.41 GPIO Configuration\n"));
        SystemConfiguration->LpsseMMCEnabled            = 1;
        SystemConfiguration->LpsseMMC45Enabled          = 0;
    } else if (SystemConfiguration->eMMCBootMode == 3) { // eMMC 4.5
         DEBUG ((DEBUG_ERROR, "Force to eMMC 4.5 GPIO Configuration\n"));
         SystemConfiguration->LpsseMMCEnabled            = 0;
         SystemConfiguration->LpsseMMC45Enabled          = 1;

    } else { // Disable eMMC controllers
         DEBUG ((DEBUG_ERROR, "Disable eMMC GPIO controllers\n"));
         SystemConfiguration->LpsseMMCEnabled            = 0;
         SystemConfiguration->LpsseMMC45Enabled          = 0;
    }

  /*
  20.1.1  EMMC
  SDMMC1_CLK -         write 0x2003ED01 to IOBASE + 0x03E0
  SDMMC1_CMD -        write 0x2003EC81 to IOBASE + 0x0390
  SDMMC1_D0 -           write 0x2003EC81 to IOBASE + 0x03D0
  SDMMC1_D1 -           write 0x2003EC81 to IOBASE + 0x0400
  SDMMC1_D2 -           write 0x2003EC81 to IOBASE + 0x03B0
  SDMMC1_D3_CD_B - write 0x2003EC81 to IOBASE + 0x0360
  MMC1_D4_SD_WE -   write 0x2003EC81 to IOBASE + 0x0380
  MMC1_D5 -                write 0x2003EC81 to IOBASE + 0x03C0
  MMC1_D6 -                write 0x2003EC81 to IOBASE + 0x0370
  MMC1_D7 -                write 0x2003EC81 to IOBASE + 0x03F0
  MMC1_RESET_B -       write 0x2003ED01 to IOBASE + 0x0330
  */
  if (SystemConfiguration->LpsseMMCEnabled== 1) {
    MmioWrite32 (IO_BASE_ADDRESS + 0x03E0, 0x2003ED01); //EMMC 4.41
    MmioWrite32 (IO_BASE_ADDRESS + 0x0390, 0x2003EC81);
    MmioWrite32 (IO_BASE_ADDRESS + 0x03D0, 0x2003EC81);
    MmioWrite32 (IO_BASE_ADDRESS + 0x0400, 0x2003EC81);
    MmioWrite32 (IO_BASE_ADDRESS + 0x03B0, 0x2003EC81);
    MmioWrite32 (IO_BASE_ADDRESS + 0x0360, 0x2003EC81);
    MmioWrite32 (IO_BASE_ADDRESS + 0x0380, 0x2003EC81);
    MmioWrite32 (IO_BASE_ADDRESS + 0x03C0, 0x2003EC81);
    MmioWrite32 (IO_BASE_ADDRESS + 0x0370, 0x2003EC81);
    MmioWrite32 (IO_BASE_ADDRESS + 0x03F0, 0x2003EC81);
    MmioWrite32 (IO_BASE_ADDRESS + 0x0330, 0x2003ED01);
  }

  /*
  eMMC 4.5 controller
  SDMMC1_CLK -         write 0x2003ED03 to IOBASE + 0x03E0
  SDMMC1_CMD -        write 0x2003EC83 to IOBASE + 0x0390
  SDMMC1_D0 -           write 0x2003EC83 to IOBASE + 0x03D0
  SDMMC1_D1 -           write 0x2003EC83 to IOBASE + 0x0400
  SDMMC1_D2 -           write 0x2003EC83 to IOBASE + 0x03B0
  SDMMC1_D3_CD_B -  write 0x2003EC83 to IOBASE + 0x0360
  MMC1_D4_SD_WE -   write 0x2003EC83 to IOBASE + 0x0380
  MMC1_D5 -                write 0x2003EC83 to IOBASE + 0x03C0
  MMC1_D6 -                write 0x2003EC83 to IOBASE + 0x0370
  MMC1_D7 -                write 0x2003EC83 to IOBASE + 0x03F0
  MMC1_RESET_B -       write 0x2003ED03 to IOBASE + 0x0330
  */
  if (SystemConfiguration->LpsseMMC45Enabled== 1) {
    MmioWrite32 (IO_BASE_ADDRESS + 0x03E0, 0x2003ED03); // EMMC 4.5
    MmioWrite32 (IO_BASE_ADDRESS + 0x0390, 0x2003EC83);
    MmioWrite32 (IO_BASE_ADDRESS + 0x03D0, 0x2003EC83);
    MmioWrite32 (IO_BASE_ADDRESS + 0x0400, 0x2003EC83);
    MmioWrite32 (IO_BASE_ADDRESS + 0x03B0, 0x2003EC83);
    MmioWrite32 (IO_BASE_ADDRESS + 0x0360, 0x2003EC83);
    MmioWrite32 (IO_BASE_ADDRESS + 0x0380, 0x2003EC83);
    MmioWrite32 (IO_BASE_ADDRESS + 0x03C0, 0x2003EC83);
    MmioWrite32 (IO_BASE_ADDRESS + 0x0370, 0x2003EC83);
    MmioWrite32 (IO_BASE_ADDRESS + 0x03F0, 0x2003EC83);
    MmioWrite32 (IO_BASE_ADDRESS + 0x0330, 0x2003ED03);

  }

//
// Change GPIOC_0 setting to allow MMIO access under Android.
//
  IoWrite32 (GPIO_BASE_ADDRESS + R_PCH_GPIO_SC_USE_SEL,
           (IoRead32(GPIO_BASE_ADDRESS + R_PCH_GPIO_SC_USE_SEL) & (UINT32)~BIT0));
  DEBUG ((DEBUG_ERROR, "ConfigureSoCGpio------------end\n"));
  return EFI_SUCCESS;
}

EFI_STATUS
MeasuredBootInit (
  IN CONST EFI_PEI_SERVICES        **PeiServices,
  IN SYSTEM_CONFIGURATION           *SystemConfiguration
  )
{
  if (SystemConfiguration->MeasuredBootEnable) {
    PcdSetBoolS (PcdMeasuredBootEnable, TRUE);
  } else {
    PcdSetBoolS (PcdMeasuredBootEnable, FALSE);
  }

  return EFI_SUCCESS;
}


EFI_STATUS
ConfigureLpssAndSccGpio (
  IN SYSTEM_CONFIGURATION        *SystemConfiguration,
  IN EFI_PLATFORM_INFO_HOB       *PlatformInfo
  )
{
  /*One time configuration to each GPIO controller PSB_CONF register should be done before starting pad configuration:
  GPIO SCORE -  write 0x01001002 to IOBASE + 0x0700
  GPIO NCORE -  write 0x01001002 to IOBASE + 0x0F00
  GPIO SSUS -    write 0x01001002 to IOBASE + 0x1700
  */
    DEBUG ((DEBUG_ERROR, "ConfigureLpssAndSccGpio------------start\n"));

  /*
  19.1.1  PWM0
  PWM0 - write 0x2003CD01 to IOBASE + 0x00A0
  19.1.2  PWM1
  PWM0 - write 0x2003CD01 to IOBASE + 0x00B0
  */
  if (SystemConfiguration->LpssPwm0Enabled== 1) {
    MmioWrite32 (IO_BASE_ADDRESS + 0x00A0, 0x2003CD01);
  } else if (SystemConfiguration->LpssPwm0Enabled== 0) {
    MmioWrite32 (IO_BASE_ADDRESS + 0x00A0, 0x2003CD00);
  }

  if (SystemConfiguration->LpssPwm1Enabled== 1) {
    MmioWrite32 (IO_BASE_ADDRESS + 0x00B0, 0x2003CC01);
  } else if (SystemConfiguration->LpssPwm1Enabled== 0) {
    MmioWrite32 (IO_BASE_ADDRESS + 0x00B0, 0x2003CD00);
  }

  /*
  19.1.3  UART1
  UART1_RXD-L -     write 0x2003CC81 to IOBASE + 0x0020
  UART1_TXD-0 -     write 0x2003CC81 to IOBASE + 0x0010
  UART1_RTS_B-1 - write 0x2003CC81 to IOBASE + 0x0000
  UART1_CTS_B-H - write 0x2003CC81 to IOBASE + 0x0040
  */
  if (SystemConfiguration->LpssHsuart0Enabled== 1) {
    MmioWrite32 (IO_BASE_ADDRESS + 0x0020, 0x2003CC81); // uart1
    MmioWrite32 (IO_BASE_ADDRESS + 0x0010, 0x2003CC81);
  if (SystemConfiguration->LpssHsuart0FlowControlEnabled== 0) {
    DEBUG ((DEBUG_ERROR, "LpssHsuart0FlowControlEnabled[0]\n"));
    MmioWrite32 (IO_BASE_ADDRESS + 0x0000, 0x2003CC80);
    MmioWrite32 (IO_BASE_ADDRESS + 0x0040, 0x2003CC80);
  } else {
    DEBUG ((DEBUG_ERROR, "LpssHsuart0FlowControlEnabled[1]\n"));
    MmioWrite32 (IO_BASE_ADDRESS + 0x0000, 0x2003CC81);
    MmioWrite32 (IO_BASE_ADDRESS + 0x0040, 0x2003CC01);//W/A HSD 4752617 0x2003CC81
    }
  } else if (SystemConfiguration->LpssHsuart0Enabled== 0) {
    MmioWrite32 (IO_BASE_ADDRESS + 0x0020, 0x2003CC80); // uart1
    MmioWrite32 (IO_BASE_ADDRESS + 0x0010, 0x2003CC80);
  }


  /*
  19.1.4  UART2
  UART2_RTS_B-1 -  write 0x2003CC81 to IOBASE + 0x0090
  UART2_CTS_B-H - write 0x2003CC81 to IOBASE + 0x0080
  UART2_RXD-H -    write 0x2003CC81 to IOBASE + 0x0060
  UART2_TXD-0 -     write 0x2003CC81 to IOBASE + 0x0070
  */
  if (SystemConfiguration->LpssHsuart1Enabled== 1) {
    MmioWrite32 (IO_BASE_ADDRESS + 0x0060, 0x2003CC81);
    MmioWrite32 (IO_BASE_ADDRESS + 0x0070, 0x2003CC81);

  if (SystemConfiguration->LpssHsuart1FlowControlEnabled== 0) {
    DEBUG ((DEBUG_ERROR, "LpssHsuart1FlowControlEnabled[0]\n"));
    MmioWrite32 (IO_BASE_ADDRESS + 0x0090, 0x2003CC80); // UART2_RTS_B
    MmioWrite32 (IO_BASE_ADDRESS + 0x0080, 0x2003CC80); // UART2_CTS_B
  } else {
    DEBUG ((DEBUG_ERROR, "LpssHsuart1FlowControlEnabled[1]\n"));
    MmioWrite32 (IO_BASE_ADDRESS + 0x0090, 0x2003CC81); // uart2
    MmioWrite32 (IO_BASE_ADDRESS + 0x0080, 0x2003CC01); //W/A HSD 4752617 0x2003CC81
  }
  } else if (SystemConfiguration->LpssHsuart1Enabled== 0) {
  MmioWrite32 (IO_BASE_ADDRESS + 0x0060, 0x2003CC80);
  MmioWrite32 (IO_BASE_ADDRESS + 0x0070, 0x2003CC80);
  }

  /*
  19.1.5  SPI
  SPI1_CS0_B - write 0x2003CC81 to IOBASE + 0x0110
  SPI1_CLK -     write 0x2003CD01 to IOBASE + 0x0100
  SPI1_MOSI -   write 0x2003CC81 to IOBASE + 0x0130
  SPI1_MISO -   write 0x2003CC81 to IOBASE + 0x0120
  */
  if (SystemConfiguration->LpssSpiEnabled== 1) {
    MmioWrite32 (IO_BASE_ADDRESS + 0x0110, 0x2003CC81); // SPI
    MmioWrite32 (IO_BASE_ADDRESS + 0x0100, 0x2003CD01);
    MmioWrite32 (IO_BASE_ADDRESS + 0x0130, 0x2003CC81);
    MmioWrite32 (IO_BASE_ADDRESS + 0x0120, 0x2003CC81);
  } else if (SystemConfiguration->LpssSpiEnabled== 0) {
    MmioWrite32 (IO_BASE_ADDRESS + 0x0110, 0x2003cc80);
    MmioWrite32 (IO_BASE_ADDRESS + 0x0100, 0x2003cc80);
    MmioWrite32 (IO_BASE_ADDRESS + 0x0130, 0x2003cc80);
    MmioWrite32 (IO_BASE_ADDRESS + 0x0120, 0x2003cc80);
  }

  /*
  19.1.6  I2C0
  I2C0_SDA-OD-O -    write 0x2003CC81 to IOBASE + 0x0210
  I2C0_SCL-OD-O -    write 0x2003CC81 to IOBASE + 0x0200
  */
  if (SystemConfiguration->LpssI2C0Enabled== 1) {
    MmioWrite32 (IO_BASE_ADDRESS + 0x0210, 0x2003C881);
    MmioWrite32 (IO_BASE_ADDRESS + 0x0200, 0x2003C881);
  }
  /*
  19.1.7  I2C1
  I2C1_SDA-OD-O/I - write 0x2003CC81 to IOBASE + 0x01F0
  I2C1_SCL-OD-O/I - write 0x2003CC81 to IOBASE + 0x01E0
  */

  if (SystemConfiguration->LpssI2C1Enabled== 1) {
    MmioWrite32 (IO_BASE_ADDRESS + 0x01F0, 0x2003C881);
    MmioWrite32 (IO_BASE_ADDRESS + 0x01E0, 0x2003C881);
  }
  /*
  19.1.8  I2C2
  I2C2_SDA-OD-O/I - write 0x2003CC81 to IOBASE + 0x01D0
  I2C2_SCL-OD-O/I - write 0x2003CC81 to IOBASE + 0x01B0
  */
  if (SystemConfiguration->LpssI2C2Enabled== 1) {
    MmioWrite32 (IO_BASE_ADDRESS + 0x01D0, 0x2003C881);
    MmioWrite32 (IO_BASE_ADDRESS + 0x01B0, 0x2003C881);
  }
  /*
  19.1.9  I2C3
  I2C3_SDA-OD-O/I - write 0x2003CC81 to IOBASE + 0x0190
  I2C3_SCL-OD-O/I - write 0x2003CC81 to IOBASE + 0x01C0
  */
  if (SystemConfiguration->LpssI2C3Enabled== 1) {
    MmioWrite32 (IO_BASE_ADDRESS + 0x0190, 0x2003C881);
    MmioWrite32 (IO_BASE_ADDRESS + 0x01C0, 0x2003C881);
  }
  /*
  19.1.10 I2C4
  I2C4_SDA-OD-O/I - write 0x2003CC81 to IOBASE + 0x01A0
  I2C4_SCL-OD-O/I - write 0x2003CC81 to IOBASE + 0x0170
  */
  if (SystemConfiguration->LpssI2C4Enabled== 1) {
    MmioWrite32 (IO_BASE_ADDRESS + 0x01A0, 0x2003C881);
    MmioWrite32 (IO_BASE_ADDRESS + 0x0170, 0x2003C881);
  }
  /*
  19.1.11 I2C5
  I2C5_SDA-OD-O/I - write 0x2003CC81 to IOBASE + 0x0150
  I2C5_SCL-OD-O/I - write 0x2003CC81 to IOBASE + 0x0140
  */
  //touch 1.7M support on i2c5(from 0) need 2k PULL-UP.
  if (SystemConfiguration->LpssI2C5Enabled== 1) {
    MmioWrite32 (IO_BASE_ADDRESS + 0x0150, 0x2003C881);
    MmioWrite32 (IO_BASE_ADDRESS + 0x0140, 0x2003C881);
  } else if(SystemConfiguration->LpssI2C5Enabled== 0) {
    MmioWrite32 (IO_BASE_ADDRESS + 0x0150, 0x2003C880);
    MmioWrite32 (IO_BASE_ADDRESS + 0x0140, 0x2003C880);
  }
  /*
  19.1.12 I2C6
  I2C6_SDA-OD-O/I - write 0x2003CC81 to IOBASE + 0x0180
  I2C6_SCL-OD-O/I -  write 0x2003CC81 to IOBASE + 0x0160
  */
  if (SystemConfiguration->LpssI2C6Enabled== 1) {
    MmioWrite32 (IO_BASE_ADDRESS + 0x0180, 0x2003C881);
    MmioWrite32 (IO_BASE_ADDRESS + 0x0160, 0x2003C881);
  } else if (SystemConfiguration->LpssI2C6Enabled== 0) {
    MmioWrite32 (IO_BASE_ADDRESS + 0x0180, 0x2003C880);
    MmioWrite32 (IO_BASE_ADDRESS + 0x0160, 0x2003C880);
  }


  /*
  20.1.2  SDIO
  SDMMC2_CLK -  write 0x2003ED01 to IOBASE + 0x0320
  SDMMC2_CMD - write 0x2003EC81 to IOBASE + 0x0300
  SDMMC2_D0 -    write 0x2003EC81 to IOBASE + 0x0350
  SDMMC2_D1 -    write 0x2003EC81 to IOBASE + 0x02F0
  SDMMC2_D2 -    write 0x2003EC81 to IOBASE + 0x0340
  SDMMC2_D3_CD_B - write 0x2003EC81 to IOBASE + 0x0310
  */
  if (SystemConfiguration->LpssSdioEnabled== 1) {
    MmioWrite32 (IO_BASE_ADDRESS + 0x0320, 0x2003ED01);//SDIO
    MmioWrite32 (IO_BASE_ADDRESS + 0x0300, 0x2003EC81);
    MmioWrite32 (IO_BASE_ADDRESS + 0x0350, 0x2003EC81);
    MmioWrite32 (IO_BASE_ADDRESS + 0x02F0, 0x2003EC81);
    MmioWrite32 (IO_BASE_ADDRESS + 0x0340, 0x2003EC81);
    MmioWrite32 (IO_BASE_ADDRESS + 0x0310, 0x2003EC81);
  }

  /*
  20.1.3  SD Card
  SDMMC3_1P8_EN - write 0x2003CD01 to IOBASE + 0x03F0
  SDMMC3_CD_B -    write 0x2003CC81 to IOBASE + 0x03A0
  SDMMC3_CLK -       write 0x2003CD01 to IOBASE + 0x02B0
  SDMMC3_CMD -      write 0x2003CC81 to IOBASE + 0x02C0
  SDMMC3_D0 -         write 0x2003CC81 to IOBASE + 0x02E0
  SDMMC3_D1 -         write 0x2003CC81 to IOBASE + 0x0290
  SDMMC3_D2 -         write 0x2003CC81 to IOBASE + 0x02D0
  SDMMC3_D3 -         write 0x2003CC81 to IOBASE + 0x02A0
  SDMMC3_PWR_EN_B - write 0x2003CC81 to IOBASE + 0x0690
  SDMMC3_WP -            write 0x2003CC82 to IOBASE + 0x0160
  */
  if (SystemConfiguration->LpssSdcardEnabled == 1) {
    if (!((PlatformInfo->BoardId == BOARD_ID_BL_FFRD && PlatformInfo->BoardRev== PR11) && (SystemConfiguration->CfioPnpSettings == 1))) {
      MmioWrite32 (IO_BASE_ADDRESS + 0x05F0, 0x2003CD01);//SDCARD
      MmioWrite32 (IO_BASE_ADDRESS + 0x02B0, 0x2003CD01);
      MmioWrite32 (IO_BASE_ADDRESS + 0x02C0, 0x2003CC81);
      MmioWrite32 (IO_BASE_ADDRESS + 0x02E0, 0x2003CC81);
      MmioWrite32 (IO_BASE_ADDRESS + 0x0290, 0x2003CC81);
      MmioWrite32 (IO_BASE_ADDRESS + 0x02D0, 0x2003CC81);
      MmioWrite32 (IO_BASE_ADDRESS + 0x02A0, 0x2003CC81);
      MmioWrite32 (IO_BASE_ADDRESS + 0x0690, 0x2003CC81);
      MmioWrite32 (IO_BASE_ADDRESS + 0x0650, 0x2003CC82); //GPIOC_7 set to WP Pin
     }
  }


     DEBUG ((DEBUG_ERROR, "ConfigureLpssAndSccGpio------------end\n"));
    return EFI_SUCCESS;
}

EFI_STATUS
ConfigureLpeGpio (
  IN SYSTEM_CONFIGURATION  *SystemConfiguration
  )
{
  DEBUG ((DEBUG_ERROR, "ConfigureLpeGpio------------start\n"));

  if (SystemConfiguration->PchAzalia == 0) {
    MmioAndThenOr32 (IO_BASE_ADDRESS + 0x220, (UINT32)~(0x7), (UINT32) (0x01));
    MmioAndThenOr32 (IO_BASE_ADDRESS + 0x250, (UINT32)~(0x7), (UINT32) (0x01));
    MmioAndThenOr32 (IO_BASE_ADDRESS + 0x240, (UINT32)~(0x7), (UINT32) (0x01));
    MmioAndThenOr32 (IO_BASE_ADDRESS + 0x260, (UINT32)~(0x7), (UINT32) (0x01));
    MmioAndThenOr32 (IO_BASE_ADDRESS + 0x270, (UINT32)~(0x7), (UINT32) (0x01));
    MmioAndThenOr32 (IO_BASE_ADDRESS + 0x230, (UINT32)~(0x7), (UINT32) (0x01));
    MmioAndThenOr32 (IO_BASE_ADDRESS + 0x280, (UINT32)~(0x7), (UINT32) (0x01));
    MmioAndThenOr32 (IO_BASE_ADDRESS + 0x540, (UINT32)~(0x7), (UINT32) (0x01));
  }

  DEBUG ((DEBUG_ERROR, "ConfigureLpeGpio------------end\n"));

  return EFI_SUCCESS;
}

EFI_STATUS
ConfigureSciSmiGpioRout (
  IN EFI_PLATFORM_INFO_HOB       *PlatformInfo)
{
	UINT32 	GPI_Routing;

	GPI_Routing = MmioRead32 (PMC_BASE_ADDRESS + R_PCH_PMC_GPI_ROUT);

	//
	// For FAB3, Route GPIO_CORE 0 to cause Runtime SCI, GPIO_SUS 0 to cause Wake SCI and GPIO_SUS 7 to cause EXTSMI
	//
	if(PlatformInfo->BoardRev == 3) {
	GPI_Routing = GPI_Routing & 0xfffc3ffc;
	GPI_Routing = GPI_Routing | 0x00024002;
	}

	//
	// For FAB2/1, Route GPIO_CORE 7 to cause Runtime SCI, GPIO_SUS 0 to cause Wake SCI and GPIO_SUS 7 to cause EXTSMI
	//
	else {
	GPI_Routing = GPI_Routing & 0x3fff3ffc;
	GPI_Routing = GPI_Routing | 0x80004002;
	}
	MmioWrite32((PMC_BASE_ADDRESS + R_PCH_PMC_GPI_ROUT), GPI_Routing);

	return EFI_SUCCESS;
}

EFI_STATUS
ConfigureMipiCsi (
  VOID)
{
	  //
    //Configure the platform clock for MIPI-CSI usage
    //PLT_CLK0
    //
    MmioAndThenOr32 (IO_BASE_ADDRESS + 0x6a0, (UINT32)~(0x7), (UINT32) (0x01));

    //
    //PLT_CLK1
    //
    MmioAndThenOr32 (IO_BASE_ADDRESS + 0x570, (UINT32)~(0x7), (UINT32) (0x01));

    //
    //PLT_CLK2
    //
    MmioAndThenOr32 (IO_BASE_ADDRESS + 0x5B0, (UINT32)~(0x7), (UINT32) (0x01));

    return EFI_SUCCESS;
}

EFI_STATUS
ConfigureUSBULPI (
  VOID)
{
	  //
    //Configure USB ULPI
    //USB_ULPI_0_CLK
    //
    MmioAndThenOr32 (IO_BASE_ADDRESS + GPIO_SSUS_OFFSET + 0x338, (UINT32)~(0x7), (UINT32) (GPI));
    MmioAndThenOr32 (IO_BASE_ADDRESS + GPIO_SSUS_OFFSET + 0x330, (UINT32)~(0x187), (UINT32) (0x101));

    //
    //USB_ULPI_0_DATA0
    //
    MmioAndThenOr32 (IO_BASE_ADDRESS + GPIO_SSUS_OFFSET + 0x388, (UINT32)~(0x7), (UINT32) (GPI));
    MmioAndThenOr32 (IO_BASE_ADDRESS + GPIO_SSUS_OFFSET + 0x380, (UINT32)~(0x187), (UINT32) (0x101));

    //
    //USB_ULPI_0_DATA1
    //
    MmioAndThenOr32 (IO_BASE_ADDRESS + GPIO_SSUS_OFFSET + 0x368, (UINT32)~(0x7), (UINT32) (GPI));
    MmioAndThenOr32 (IO_BASE_ADDRESS + GPIO_SSUS_OFFSET + 0x360, (UINT32)~(0x187), (UINT32) (0x101));

    //
    //USB_ULPI_0_DATA2
    //
    MmioAndThenOr32 (IO_BASE_ADDRESS + GPIO_SSUS_OFFSET + 0x318, (UINT32)~(0x7), (UINT32) (GPI));
    MmioAndThenOr32 (IO_BASE_ADDRESS + GPIO_SSUS_OFFSET + 0x310, (UINT32)~(0x187), (UINT32) (0x101));

    //
    //USB_ULPI_0_DATA3
    //
    MmioAndThenOr32 (IO_BASE_ADDRESS + GPIO_SSUS_OFFSET + 0x378, (UINT32)~(0x7), (UINT32) (GPI));
    MmioAndThenOr32 (IO_BASE_ADDRESS + GPIO_SSUS_OFFSET + 0x370, (UINT32)~(0x187), (UINT32) (0x101));

    //
    //USB_ULPI_0_DATA4
    //
    MmioAndThenOr32 (IO_BASE_ADDRESS + GPIO_SSUS_OFFSET + 0x308, (UINT32)~(0x7), (UINT32) (GPI));
    MmioAndThenOr32 (IO_BASE_ADDRESS + GPIO_SSUS_OFFSET + 0x300, (UINT32)~(0x187), (UINT32) (0x101));

    //
    //USB_ULPI_0_DATA5
    //
    MmioAndThenOr32 (IO_BASE_ADDRESS + GPIO_SSUS_OFFSET + 0x398, (UINT32)~(0x7), (UINT32) (GPI));
    MmioAndThenOr32 (IO_BASE_ADDRESS + GPIO_SSUS_OFFSET + 0x390, (UINT32)~(0x187), (UINT32) (0x101));

    //
    //USB_ULPI_0_DATA6
    //
    MmioAndThenOr32 (IO_BASE_ADDRESS + GPIO_SSUS_OFFSET + 0x328, (UINT32)~(0x7), (UINT32) (GPI));
    MmioAndThenOr32 (IO_BASE_ADDRESS + GPIO_SSUS_OFFSET + 0x320, (UINT32)~(0x187), (UINT32) (0x101));

    //
    //USB_ULPI_0_DATA7
    //
    MmioAndThenOr32 (IO_BASE_ADDRESS + GPIO_SSUS_OFFSET + 0x3a8, (UINT32)~(0x7), (UINT32) (GPI));
    MmioAndThenOr32 (IO_BASE_ADDRESS + GPIO_SSUS_OFFSET + 0x3a0, (UINT32)~(0x187), (UINT32) (0x101));

    //
    //USB_ULPI_0_DIR
    //
    MmioAndThenOr32 (IO_BASE_ADDRESS + GPIO_SSUS_OFFSET + 0x348, (UINT32)~(0x7), (UINT32) (GPI));
    MmioAndThenOr32 (IO_BASE_ADDRESS + GPIO_SSUS_OFFSET + 0x340, (UINT32)~(0x187), (UINT32) (0x81));

    //
    //USB_ULPI_0_NXT
    //
    MmioAndThenOr32 (IO_BASE_ADDRESS + GPIO_SSUS_OFFSET + 0x358, (UINT32)~(0x7), (UINT32) (GPI));
    MmioAndThenOr32 (IO_BASE_ADDRESS + GPIO_SSUS_OFFSET + 0x350, (UINT32)~(0x187), (UINT32) (0x101));

    //
    //USB_ULPI_0_STP
    //
    MmioAndThenOr32 (IO_BASE_ADDRESS + GPIO_SSUS_OFFSET + 0x3b8, (UINT32)~(0x7), (UINT32) (GPI));
    MmioAndThenOr32 (IO_BASE_ADDRESS + GPIO_SSUS_OFFSET + 0x3b0, (UINT32)~(0x187), (UINT32) (0x81));

    //
    //USB_ULPI_0_REFCLK
    //
    MmioAndThenOr32 (IO_BASE_ADDRESS + GPIO_SSUS_OFFSET + 0x288, (UINT32)~(0x7), (UINT32) (GPI));
    MmioAndThenOr32 (IO_BASE_ADDRESS + GPIO_SSUS_OFFSET + 0x280, (UINT32)~(0x187), (UINT32) (0x101));

    return EFI_SUCCESS;
}

EFI_STATUS
DisableRTD3 (
  VOID)
{
	  //
    //Disable RTD3
    //
    MmioAndThenOr32 (IO_BASE_ADDRESS + GPIO_SSUS_OFFSET + 0x210, (UINT32)~(0x0f000007), (UINT32) (0x00));
    MmioAndThenOr32 (IO_BASE_ADDRESS + GPIO_SSUS_OFFSET + 0x1e0, (UINT32)~(0x0f000007), (UINT32) (0x00));

    return EFI_SUCCESS;
}

/**
  Platform specific initializations in stage1.

  @param FfsHeader         Pointer to the PEIM FFS file header.
  @param PeiServices       General purpose services available to every PEIM.

  @retval EFI_SUCCESS       Operation completed successfully.
  @retval Otherwise         Platform initialization failed.
**/
EFI_STATUS
EFIAPI
PlatformEarlyInitEntry (

  IN       EFI_PEI_FILE_HANDLE  FileHandle,
  IN CONST EFI_PEI_SERVICES    **PeiServices
  )
{
  EFI_STATUS                  Status;
  SYSTEM_CONFIGURATION        SystemConfiguration;
  EFI_PLATFORM_INFO_HOB       *PlatformInfo;
  EFI_PEI_HOB_POINTERS        Hob;
  EFI_PLATFORM_CPU_INFO       PlatformCpuInfo;

  //
  // Initialize SmbusPolicy PPI
  //
  Status = (*PeiServices)->InstallPpi(PeiServices, &mInstallSmbusPolicyPpi);
  ASSERT_EFI_ERROR (Status);

  //
  // Initialize Stall PPIs
  //
  Status = (*PeiServices)->InstallPpi (PeiServices, &mInstallStallPpi);
  ASSERT_EFI_ERROR (Status);

  //
  // Initialize platform PPIs
  //
  Status = (*PeiServices)->InstallPpi (PeiServices, &mInstallSpeakerInterfacePpi);
  ASSERT_EFI_ERROR (Status);

  //
  // Variable initialization
  //
  ZeroMem(&PlatformCpuInfo, sizeof(EFI_PLATFORM_CPU_INFO));

  //
  // Set the some PCI and chipset range as UC
  // And align to 1M at leaset
  //
  Hob.Raw = GetFirstGuidHob (&gEfiPlatformInfoGuid);
  ASSERT (Hob.Raw != NULL);
  PlatformInfo = GET_GUID_HOB_DATA(Hob.Raw);

  //
  // Initialize PlatformInfo HOB
  //
  MultiPlatformInfoInit(PeiServices, PlatformInfo);

  //
  // Do basic MCH init
  //
  MchInit (PeiServices);

  //
  // Set the new boot mode
  //
  Status = UpdateBootMode (PeiServices, PlatformInfo);
  ASSERT_EFI_ERROR (Status);

  SetPlatformBootMode (PeiServices, PlatformInfo);

  //
  // Get setup variable. This can only be done after BootMode is updated
  //
  Status = GetSetupVariable (PeiServices, &SystemConfiguration);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_INFO, "PlatformEarlyInitEntry: Setup Variable does not exist.\n"));
    DEBUG ((DEBUG_INFO, "PlatformEarlyInitEntry: Set PcdNvStoreDefaultId to STANDARD\n"));
    //
    // Set the default NV store to STANDARD defaults
    //
    PcdSet16S (PcdSetNvStoreDefaultId, 0x0000);

    //
    // Get setup variable. Must succeed at this point.
    //
    Status = GetSetupVariable (PeiServices, &SystemConfiguration);
    ASSERT_EFI_ERROR (Status);
  }

  CheckOsSelection(PeiServices, &SystemConfiguration);

  //
  // Update PlatformInfo HOB according to setup variable
  //
  PlatformInfoUpdate(PeiServices, PlatformInfo, &SystemConfiguration);

  InitializePlatform (PeiServices, PlatformInfo, &SystemConfiguration);

  //
  // Initialize VlvPolicy PPI
  //
  Status = VlvPolicyInit (PeiServices, &SystemConfiguration);
  ASSERT_EFI_ERROR (Status);

  //
  // Soc specific GPIO setting
  //
  ConfigureSoCGpio(&SystemConfiguration);

  //
  //  Baylake Board specific.
  //
  if (PlatformInfo->BoardId == BOARD_ID_BL_RVP  ||
      PlatformInfo->BoardId == BOARD_ID_BL_FFRD ||
	    PlatformInfo->BoardId == BOARD_ID_BL_FFRD8 ||
      PlatformInfo->BoardId == BOARD_ID_BL_RVP_DDR3L ||
      PlatformInfo->BoardId == BOARD_ID_BL_STHI ||
      PlatformInfo->BoardId == BOARD_ID_BB_RVP ||
      PlatformInfo->BoardId == BOARD_ID_BS_RVP ||
      PlatformInfo->BoardId == BOARD_ID_MINNOW2 ||
      PlatformInfo->BoardId == BOARD_ID_MINNOW2_TURBOT||
      PlatformInfo->BoardId == BOARD_ID_CVH) {
    ConfigureLpssAndSccGpio(&SystemConfiguration, PlatformInfo);

  }


  //
  //  Configure LPE
  //  Alpine Valley and Bayley Bay board specific
  //
  ConfigureLpeGpio(&SystemConfiguration);

  //
  //  Bayley Bay Board specific.
  //
  ConfigureSciSmiGpioRout(PlatformInfo);
  if (SystemConfiguration.LpssI2C3Enabled == 1) {
    ConfigureMipiCsi();
  }


  //
  // Do basic CPU init
  //
  Status = PlatformCpuInit (PeiServices, &SystemConfiguration, &PlatformCpuInfo);

  //
  // Perform basic SSA related platform initialization
  //
  PlatformSsaInit (&SystemConfiguration,PeiServices);


  //
  // Do basic PCH init
  //
  Status = PlatformPchInit (&SystemConfiguration, PeiServices, PlatformInfo->PlatformType);
  ASSERT_EFI_ERROR (Status);

  //
  // Initialize platform PPIs
  //
  Status = (*PeiServices)->InstallPpi (PeiServices, &mPpiList[0]);
  ASSERT_EFI_ERROR (Status);

  if (PlatformInfo->BoardId != BOARD_ID_CVH) {
    InstallPlatformClocksNotify (PeiServices);
    InstallPlatformSysCtrlGPIONotify(PeiServices);
  }

  //
  // Initialize platform PPIs
  //
  Status = (*PeiServices)->NotifyPpi(PeiServices, &mNotifyList[0]);
  ASSERT_EFI_ERROR (Status);

  //
  // Initialize Measured Boot
  //
  Status = MeasuredBootInit (PeiServices, &SystemConfiguration);
  ASSERT_EFI_ERROR (Status);

  return Status;
}
