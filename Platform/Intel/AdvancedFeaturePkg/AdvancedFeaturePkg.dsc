## @file
#  Advanced Feature Package build description file.
#
# Copyright (c) 2017 - 2019, Intel Corporation. All rights reserved.<BR>
#
# SPDX-License-Identifier: BSD-2-Clause-Patent
#
##

################################################################################
#
# Defines Section - statements that will be processed to create a Makefile.
#
################################################################################
[Defines]
  PLATFORM_NAME                       = AdvancedFeaturePkg
  PLATFORM_GUID                       = 5E4C05BC-C5F0-4843-BAE1-3AAFE269DB8F
  PLATFORM_VERSION                    = 0.1
  DSC_SPECIFICATION                   = 0x00010005
  OUTPUT_DIRECTORY                    = Build/AdvancedFeaturePkg
  SUPPORTED_ARCHITECTURES             = IA32|X64
  BUILD_TARGETS                       = DEBUG|RELEASE
  SKUID_IDENTIFIER                    = DEFAULT
