#
#  Copyright (c) 2024, Arm Limited. All rights reserved.
#
#  SPDX-License-Identifier: BSD-2-Clause-Patent
#

################################################################################
#
# Defines Section - statements that will be processed to create a Makefile.
#
################################################################################
[Defines]
  PLATFORM_NAME                  = RdFremont
  PLATFORM_GUID                  = fd140b0f-4467-4314-aa69-cd0bd712e08e
  PLATFORM_VERSION               = 0.1
  DSC_SPECIFICATION              = 0x0001001B
  OUTPUT_DIRECTORY               = Build/$(PLATFORM_NAME)
  SUPPORTED_ARCHITECTURES        = AARCH64
  BUILD_TARGETS                  = NOOPT|DEBUG|RELEASE
  SKUID_IDENTIFIER               = DEFAULT
  FLASH_DEFINITION               = Platform/ARM/SgiPkg/SgiPlatform.fdf
  BOARD_DXE_FV_COMPONENTS        = Platform/ARM/SgiPkg/RdFremont/RdFremont.fdf.inc
  BUILD_NUMBER                   = 1

# include common definitions from SgiPlatform.dsc
!include Platform/ARM/SgiPkg/SgiPlatform.dsc.inc
!include Platform/ARM/SgiPkg/SgiMemoryMap3.dsc.inc

# include common/basic libraries from MdePkg.
!include MdePkg/MdeLibs.dsc.inc

################################################################################
#
# Pcd Section - list of all EDK II PCD Entries defined by this Platform
#
################################################################################

[PcdsFixedAtBuild.common]
  # GIC Base Addresses
  gArmTokenSpaceGuid.PcdGicDistributorBase|0x30000000
  gArmTokenSpaceGuid.PcdGicRedistributorsBase|0x301C0000
  gArmSgiTokenSpaceGuid.PcdGicSize|0x200000

  # ARM Cores and Clusters
  gArmPlatformTokenSpaceGuid.PcdCoreCount|1
  gArmPlatformTokenSpaceGuid.PcdClusterCount|16

################################################################################
#
# Components Section - list of all EDK II Modules needed by this Platform
#
################################################################################

[Components.common]
  Platform/ARM/SgiPkg/AcpiTables/RdFremontAcpiTables.inf
