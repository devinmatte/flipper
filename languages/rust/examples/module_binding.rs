extern crate flipper;

use flipper::{Flipper, Module, ModuleFFI, UserModule, UserModuleFFI};
use flipper::fmr::{FmrInvocation, FmrReturn};

struct GpioToggle {
    ffi: ModuleFFI,
}

impl Module for GpioToggle {
    fn new() -> Self {
        GpioToggle {
            ffi: ModuleFFI::User(UserModuleFFI::uninitialized(Self::name())),
        }
    }
}

impl From<UserModuleFFI> for GpioToggle {
    fn from(user: UserModuleFFI) -> Self {
        GpioToggle {
            ffi: ModuleFFI::User(user),
        }
    }
}

impl UserModule for GpioToggle {
    fn name<'a>() -> &'a str { "gpio" }
}

impl GpioToggle {
    fn toggle(&self) {
        FmrInvocation::new(&self.ffi, "toggle", 0, FmrReturn::Unit)
            .append(4u8)
            .invoke();
    }
}

fn main() {
    let flipper = Flipper::attach();
    let gpio: GpioToggle = GpioToggle::bind(&flipper);
    gpio.toggle();
}