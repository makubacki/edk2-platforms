/** @file
  ACPI DSDT table

Copyright (c) 2019, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials are licensed and made available under
the terms and conditions of the BSD License that accompanies this distribution.
The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php.

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/


// LPC Bridge - Device 31, Function 0
scope (\_SB.PCI0.LPCB) {
      Include ("LPC_DEV.ASL")

      // Define the KBC_COMMAND_REG-64, KBC_DATA_REG-60 Registers as an ACPI Operating
      // Region.  These registers will be used to skip kbd mouse
      // resource settings if not present.
      OperationRegion(PKBS, SystemIO, 0x60, 0x05)
      Field(PKBS, ByteAcc, Lock, Preserve)
      {
        PKBD, 8,
        , 8,
        , 8,
        , 8,
        PKBC, 8
      }
      Device(PS2K)    // PS2 Keyboard
      {
        Name(_HID,"MSFT0001")
        Name(_CID,EISAID("PNP0303"))

        Method(_STA)
        {
          If (P2MK) //Ps2 Keyboard and Mouse Enable
          {
            Return(0x000F)
          }
          Return(0x0000)
        }

        Name(_CRS,ResourceTemplate()
        {
          IO(Decode16,0x60,0x60,0x01,0x01)
          IO(Decode16,0x64,0x64,0x01,0x01)
          IRQ(Edge,ActiveHigh,Exclusive){0x01}
        })

        Name(_PRS, ResourceTemplate(){
          StartDependentFn(0, 0) {
          FixedIO(0x60,0x01)
          FixedIO(0x64,0x01)
          IRQNoFlags(){1}
          }
          EndDependentFn()
        })

      }

      Device(PS2M)    // PS/2 Mouse
      {
        Name(_HID,"MSFT0003")
        Name(_CID,EISAID("PNP0F03"))

        Method(_STA)
        {
          If (P2ME) //Ps2 Mouse Enable
          {
            If (P2MK) //Ps2 Keyboard and Mouse Enable
            {
              Return(0x000F)
            }
          }
          Return(0x0000)
        }

        Name(_CRS,ResourceTemplate()
        {
          IRQ(Edge,ActiveHigh,Exclusive){0x0C}
        })

        Name(_PRS, ResourceTemplate(){
          StartDependentFn(0, 0) {
          IRQNoFlags(){12}
          }
          EndDependentFn()
        })
      }

}
