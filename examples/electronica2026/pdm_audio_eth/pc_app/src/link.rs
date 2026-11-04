// SPDX-License-Identifier: BSD-3-Clause
// Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/

//! Raw-Ethernet link to the board (design 7): AF_PACKET socket on the
//! interface, bound to EtherType 0x88B5.

use std::io;
use std::path::Path;
use std::time::Duration;

use crate::proto::ETHERTYPE;

/// One received frame: bytes from the destination MAC on (no CRC), and the
/// PC's CLOCK_MONOTONIC time in ns when it was read.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct RxFrame {
    pub data: Vec<u8>,
    pub t_ns: u64,
}

/// Counters of the PC side of the link.
#[derive(Clone, Copy, Debug, Default, PartialEq, Eq)]
pub struct LinkStats {
    /// Frames the kernel dropped because the socket buffer was full
    /// (PACKET_STATISTICS tp_drops, accumulated).
    pub rx_kernel_drops: u64,
}

/// A raw-Ethernet link to the board (real NIC or the emulator).
pub trait Link: Send {
    /// Send one complete frame (destination MAC first, no CRC).
    fn send(&mut self, frame: &[u8]) -> io::Result<()>;
    /// Wait up to `timeout` for one frame of this protocol.
    fn recv(&mut self, timeout: Duration) -> io::Result<Option<RxFrame>>;
    /// The PC side's MAC address.
    fn mac(&self) -> [u8; 6];
    /// PC-side counters.
    fn stats(&mut self) -> LinkStats;
}

/// CLOCK_MONOTONIC in ns.
pub fn now_ns() -> u64 {
    let mut ts = libc::timespec { tv_sec: 0, tv_nsec: 0 };
    unsafe { libc::clock_gettime(libc::CLOCK_MONOTONIC, &mut ts) };
    ts.tv_sec as u64 * 1_000_000_000 + ts.tv_nsec as u64
}

/// `(Mbit/s, full duplex, carrier)` of `iface` from sysfs. A down interface
/// reads as `(0, false, false)`.
pub fn link_info(iface: &str) -> io::Result<(u32, bool, bool)> {
    link_info_at(Path::new("/sys/class/net"), iface)
}

/// [`link_info`] with an injectable sysfs root (tests).
pub fn link_info_at(root: &Path, iface: &str) -> io::Result<(u32, bool, bool)> {
    let d = root.join(iface);
    if !d.is_dir() {
        return Err(io::Error::new(io::ErrorKind::NotFound, format!("no interface {iface}")));
    }
    let read = |f: &str| std::fs::read_to_string(d.join(f)).ok().map(|s| s.trim().to_string());
    let carrier = read("carrier").as_deref() == Some("1");
    let speed = read("speed").and_then(|s| s.parse::<i64>().ok()).filter(|v| *v > 0).unwrap_or(0);
    let full = read("duplex").as_deref() == Some("full");
    Ok((speed as u32, full, carrier))
}

/// True if this process has CAP_NET_RAW in its effective set.
pub fn has_net_raw() -> bool {
    const CAP_NET_RAW: u32 = 13;
    std::fs::read_to_string("/proc/self/status")
        .ok()
        .and_then(|s| {
            s.lines()
                .find_map(|l| l.strip_prefix("CapEff:"))
                .and_then(|v| u64::from_str_radix(v.trim(), 16).ok())
        })
        .is_some_and(|caps| caps & (1u64 << CAP_NET_RAW) != 0)
}

fn setcap_hint(iface: &str) -> String {
    let exe = std::env::current_exe()
        .map(|p| p.display().to_string())
        .unwrap_or_else(|_| "pdm-eth".into());
    format!(
        "raw socket on {iface} needs CAP_NET_RAW: run  sudo setcap cap_net_raw,cap_net_admin+ep {exe}  once after each build"
    )
}

/// AF_PACKET socket on one interface, bound to EtherType 0x88B5.
pub struct AfPacketLink {
    fd: libc::c_int,
    ifindex: libc::c_int,
    mac: [u8; 6],
    drops: u64,
}

const PACKET_IGNORE_OUTGOING: libc::c_int = 23;

fn last_err() -> io::Error {
    io::Error::last_os_error()
}

impl AfPacketLink {
    /// Open the link on `iface` (for example `enp129s0`).
    pub fn open(iface: &str) -> io::Result<Self> {
        let proto = (ETHERTYPE).to_be() as libc::c_int;
        let fd = unsafe { libc::socket(libc::AF_PACKET, libc::SOCK_RAW | libc::SOCK_CLOEXEC, proto) };
        if fd < 0 {
            let e = last_err();
            return Err(match e.raw_os_error() {
                Some(libc::EPERM) | Some(libc::EACCES) => {
                    io::Error::new(io::ErrorKind::PermissionDenied, setcap_hint(iface))
                }
                _ => e,
            });
        }
        let mut link = AfPacketLink { fd, ifindex: 0, mac: [0; 6], drops: 0 };
        let name = std::ffi::CString::new(iface)
            .map_err(|_| io::Error::new(io::ErrorKind::InvalidInput, "bad interface name"))?;
        link.ifindex = unsafe { libc::if_nametoindex(name.as_ptr()) } as libc::c_int;
        if link.ifindex == 0 {
            return Err(io::Error::new(io::ErrorKind::NotFound, format!("no interface {iface}")));
        }
        let mut sll: libc::sockaddr_ll = unsafe { std::mem::zeroed() };
        sll.sll_family = libc::AF_PACKET as u16;
        sll.sll_protocol = (ETHERTYPE).to_be();
        sll.sll_ifindex = link.ifindex;
        let r = unsafe {
            libc::bind(fd, &sll as *const _ as *const libc::sockaddr,
                       std::mem::size_of::<libc::sockaddr_ll>() as u32)
        };
        if r < 0 {
            return Err(last_err());
        }
        let one: libc::c_int = 1;
        let buf: libc::c_int = 8 << 20;
        unsafe {
            libc::setsockopt(fd, libc::SOL_PACKET, PACKET_IGNORE_OUTGOING,
                             &one as *const _ as *const libc::c_void, 4);
            if libc::setsockopt(fd, libc::SOL_SOCKET, libc::SO_RCVBUFFORCE,
                                &buf as *const _ as *const libc::c_void, 4) != 0 {
                libc::setsockopt(fd, libc::SOL_SOCKET, libc::SO_RCVBUF,
                                 &buf as *const _ as *const libc::c_void, 4);
            }
        }
        let mut ifr: libc::ifreq = unsafe { std::mem::zeroed() };
        for (d, s) in ifr.ifr_name.iter_mut().zip(iface.bytes()) {
            *d = s as libc::c_char;
        }
        if unsafe { libc::ioctl(fd, libc::SIOCGIFHWADDR, &mut ifr) } < 0 {
            return Err(last_err());
        }
        let hw = unsafe { ifr.ifr_ifru.ifru_hwaddr.sa_data };
        for (d, s) in link.mac.iter_mut().zip(hw.iter()) {
            *d = *s as u8;
        }
        Ok(link)
    }
}

impl Drop for AfPacketLink {
    fn drop(&mut self) {
        unsafe { libc::close(self.fd) };
    }
}

impl Link for AfPacketLink {
    fn send(&mut self, frame: &[u8]) -> io::Result<()> {
        let n = unsafe { libc::send(self.fd, frame.as_ptr() as *const libc::c_void, frame.len(), 0) };
        if n < 0 {
            return Err(last_err());
        }
        Ok(())
    }

    fn recv(&mut self, timeout: Duration) -> io::Result<Option<RxFrame>> {
        let deadline = now_ns() + timeout.as_nanos() as u64;
        let mut buf = vec![0u8; 2048];
        loop {
            let left_ms = deadline.saturating_sub(now_ns()).div_ceil(1_000_000) as libc::c_int;
            let mut pfd = libc::pollfd { fd: self.fd, events: libc::POLLIN, revents: 0 };
            let r = unsafe { libc::poll(&mut pfd, 1, left_ms) };
            if r < 0 {
                let e = last_err();
                if e.kind() == io::ErrorKind::Interrupted {
                    continue;
                }
                return Err(e);
            }
            if r == 0 {
                return Ok(None);
            }
            let mut sll: libc::sockaddr_ll = unsafe { std::mem::zeroed() };
            let mut sl = std::mem::size_of::<libc::sockaddr_ll>() as libc::socklen_t;
            let n = unsafe {
                libc::recvfrom(self.fd, buf.as_mut_ptr() as *mut libc::c_void, buf.len(), 0,
                               &mut sll as *mut _ as *mut libc::sockaddr, &mut sl)
            };
            let t_ns = now_ns();
            if n < 0 {
                return Err(last_err());
            }
            if sll.sll_pkttype == libc::PACKET_OUTGOING {
                if now_ns() >= deadline {
                    return Ok(None);
                }
                continue; // our own frame, on kernels without PACKET_IGNORE_OUTGOING
            }
            buf.truncate(n as usize);
            return Ok(Some(RxFrame { data: buf, t_ns }));
        }
    }

    fn mac(&self) -> [u8; 6] {
        self.mac
    }

    fn stats(&mut self) -> LinkStats {
        let mut st = libc::tpacket_stats { tp_packets: 0, tp_drops: 0 };
        let mut len = std::mem::size_of::<libc::tpacket_stats>() as libc::socklen_t;
        let r = unsafe {
            libc::getsockopt(self.fd, libc::SOL_PACKET, libc::PACKET_STATISTICS,
                             &mut st as *mut _ as *mut libc::c_void, &mut len)
        };
        if r == 0 {
            self.drops += st.tp_drops as u64; // the kernel resets them on read
        }
        LinkStats { rx_kernel_drops: self.drops }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn link_info_parses_sysfs() {
        let root = tempfile::tempdir().unwrap();
        let d = root.path().join("enpX");
        std::fs::create_dir_all(&d).unwrap();
        std::fs::write(d.join("speed"), "1000\n").unwrap();
        std::fs::write(d.join("duplex"), "full\n").unwrap();
        std::fs::write(d.join("carrier"), "1\n").unwrap();
        assert_eq!(link_info_at(root.path(), "enpX").unwrap(), (1000, true, true));
        std::fs::write(d.join("carrier"), "0\n").unwrap();
        std::fs::write(d.join("duplex"), "half\n").unwrap();
        assert_eq!(link_info_at(root.path(), "enpX").unwrap(), (1000, false, false));
    }

    #[test]
    fn link_info_down_interface_reads_as_no_carrier() {
        // a down interface gives EINVAL on `speed`; report 0 Mbit/s, no carrier
        let root = tempfile::tempdir().unwrap();
        let d = root.path().join("enpY");
        std::fs::create_dir_all(&d).unwrap();
        std::fs::write(d.join("carrier"), "0\n").unwrap();
        assert_eq!(link_info_at(root.path(), "enpY").unwrap(), (0, false, false));
    }

    #[test]
    fn open_without_cap_gives_setcap_hint() {
        if unsafe { libc::geteuid() } == 0 || has_net_raw() {
            return; // only meaningful without the capability
        }
        let e = AfPacketLink::open("lo").err().expect("open must fail without CAP_NET_RAW");
        let msg = e.to_string();
        assert!(msg.contains("setcap"), "{msg}");
        assert!(msg.contains("cap_net_raw"), "{msg}");
    }
}
