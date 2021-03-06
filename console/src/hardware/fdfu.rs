//! Flipper Device Firmware Upgrade
//!
//! The `fdfu` crate defines how to flash new firmware onto Flipper's hardware.
//! It utilizes Flipper's own Rust language bindings to perform the necessary
//! GPIO and UART hardware interactions to enter update mode, copy the firmware,
//! and reboot the board.

use std::io::{Read, Write, Cursor};
use std::fs::File;
use std::ffi::OsStr;
use std::path::{Path, PathBuf};
use std::{thread, time};
use std::io::Error as IoError;
use byteorder::{BigEndian, ReadBytesExt};
use xmodem::Xmodem;
use failure::{Fail, Error};

use flipper;
use flipper::fsm::{uart0, gpio};

#[derive(Debug, Fail)]
enum FdfuError {
    /// Indicates that an io::Error occurred involving a given file.
    /// _0: The name of the file involved.
    /// _1: The io::Error with details.
    #[fail(display = "File error: {}", _0)]
    FileError(IoError),
}

struct SamBa<'a, B: 'a> where B: Write + Read {
    bus: &'a mut B,
}

impl <'a, B> SamBa<'a, B> where B: Write + Read {

    pub fn jump(&mut self, address: u32) {
        write!(self.bus, "G{:08x}#", address);
    }

    pub fn write_byte(&mut self, address: u32, byte: u8) {
        write!(self.bus, "O{:08x},{:02x}#", address, byte);
    }

    pub fn write_word(&mut self, address: u32, word: u32) {
        write!(self.bus, "W{:08x},{:08x}#", address, word);
    }

    pub fn write_efc_fcr(&mut self, command: u8, arg: u32) {
        let efc_base_address: u32 = 0x400E0A00;                                          // SAM4S16.h:7479
        let eefc_fcr_offset = 0x04;                                                      // SAM4S16.h:1125

        let eefc_fcr_fkey_pos = 24;                                                      // SAM4S16.h:1144
        let eefc_fcr_fkey_msk = 0xff << eefc_fcr_fkey_pos;                               // SAM4S16.h:1145
        let eefc_fcr_fkey = eefc_fcr_fkey_msk & ((0x5A) << eefc_fcr_fkey_pos);           // SAM4S16.h:1146
        let eefc_fcr_farg = eefc_fcr_fkey_msk & ((arg) << eefc_fcr_fkey_pos);            // SAM4S16.h:1143
        let eefc_fcr_fcmd = eefc_fcr_fkey_msk & ((command as u32) << eefc_fcr_fkey_pos); // SAM4S16.h:1140

        let eefc_word: u32 = eefc_fcr_fkey | eefc_fcr_farg | eefc_fcr_fcmd;
        let eefc_address = efc_base_address + eefc_fcr_offset;

        &self.write_word(efc_base_address, eefc_word);
    }

    pub fn read_byte(&mut self, address: u32) -> u8 {
        write!(self.bus, "o{:08x},#", address);

        let mut buffer = [0u8; 1];
        &self.bus.read(&mut buffer[..]);
        buffer[0]
    }

    pub fn read_word(&mut self, address: u32) -> u32 {
        write!(self.bus, "w{:08x},#", address);
        let mut buffer = [0u8; 4];
        &self.bus.read(&mut buffer[..]);
        Cursor::new(buffer).read_u32::<BigEndian>().unwrap()
    }

}

/// Moves data from the host to the device's RAM using the SAM-BA
/// and XMODEM protocol.
fn sam_ba_copy<B: Write + Read>(bus: &mut B, address: u32, data: &mut [u8]) {

    write!(bus, "S{:08X},{:08X}#", address, data.len());

//    for _ in 0..8 { if uart0::ready() { break; } }

    let mut buffer = [0u8; 1];
    bus.read(&mut buffer[..]);
    let byte = buffer[0];
    if byte == b'C' { return; }

//    let mut xmodem = Xmodem::new();
//    xmodem.send(bus, data);
}

const SAM_ERASE_PIN: u32 = 0x06;
const SAM_RESET_PIN: u32 = 0x05;

fn sam_reset() {
    gpio::write(0, (1 << SAM_RESET_PIN));
    thread::sleep(time::Duration::from_millis(10));
    gpio::write((1 << SAM_RESET_PIN), 0);
}

fn sam_enter_dfu() -> bool {
    gpio::write((1 << SAM_ERASE_PIN), (1 << SAM_RESET_PIN));
    thread::sleep(time::Duration::from_millis(5000));
    gpio::write((1 << SAM_RESET_PIN), (1 << SAM_ERASE_PIN));
    sam_reset();
    return true;
}

fn load_page_data() {

}

fn enter_update_mode<B: Write + Read>(bus: &mut B) -> bool {

    println!("Setting uart0 to DFU baud");
    // FIXME
//    uart0::dfu();
    println!("Uart0 set to DFU baud");

    bus.write(&[b'#']);
    let mut buffer = [0u8; 3];
    bus.read(&mut buffer);

    if &buffer[..] == &[b'\n', b'\r', b'>'] {
        println!("Already in update mode");
        true
    } else {
        if sam_enter_dfu() {
            println!("Successfully entered update mode");
            true
        } else {
            println!("Failed to enter update mode");
            false
        }
    }
}

fn enter_normal_mode() {

}

/// Given a path to a file, flash the binary contents of that file as an image
/// onto Flipper's hardware.
pub fn flash<P: AsRef<Path>>(path: P) -> Result<(), Error> {
    let file = File::open(&path)
        .map_err(|e| FdfuError::FileError(e))?;

    let flipper = flipper::Flipper::attach_hostname("localhost");
//    let mut bus = flipper::fsm::uart0::Uart0::new(&flipper);
//    flipper::fsm::uart0::configure();

//    let samba = SamBa { bus: &mut bus };

    Ok(())
}

#[cfg(test)]
mod tests {
    #[test]
    fn it_works() {
    }
}
