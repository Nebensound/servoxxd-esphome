"""
ServoXXD Component for ESPHome
Supports MKS ServoXXD (28D/35D/42D/57D) closed-loop stepper motors
Communication layer is implemented via separate transport classes (Modbus, Serial, etc.)
"""

CODEOWNERS = ["@jowgn"]
DEPENDENCIES = ["uart"]
AUTO_LOAD = ["modbus"]

# The actual implementation is in the stepper subcomponent
# This allows the component to be used as: stepper.servoxxd
# Communication transport is handled by *_modbus files
