// SPDX-License-Identifier: BSD-3-Clause
// Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/

//! Board-run diagnostic: starts one TPUT_TX burst and records every frame
//! the NIC passes up in promiscuous mode (any destination, any EtherType),
//! then prints the TPUT and AUDIO_OUT sequence gaps with the frames around
//! each gap. Needs `setcap cap_net_raw,cap_net_admin+ep` on the binary.
//!
//! `cargo build --release --example rawdiag`
//! `target/release/examples/rawdiag enp129s0 [frames]`

use std::time::{Duration, Instant};

use pdm_eth::proto::*;

const ETH_P_ALL: u16 = 0x0003;

struct Rx {
    t_ns: u128,
    len: usize,
    dst: [u8; 6],
    ethertype: u16,
    kind: String,
    seq: Option<u32>,
}

fn mac(iface: &str) -> [u8; 6] {
    let s = std::fs::read_to_string(format!("/sys/class/net/{iface}/address")).expect("iface address");
    let v: Vec<u8> = s.trim().split(':').map(|x| u8::from_str_radix(x, 16).unwrap()).collect();
    v.try_into().unwrap()
}

fn main() {
    let args: Vec<String> = std::env::args().collect();
    let iface = args.get(1).map(String::as_str).unwrap_or("enp129s0");
    let count: u32 = args.get(2).map(|s| s.parse().unwrap()).unwrap_or(243_822);
    let my_mac = mac(iface);
    let ifindex = unsafe { libc::if_nametoindex(std::ffi::CString::new(iface).unwrap().as_ptr()) } as i32;
    let fd = unsafe { libc::socket(libc::AF_PACKET, libc::SOCK_RAW, ETH_P_ALL.to_be() as i32) };
    assert!(fd >= 0, "socket: {} (setcap?)", std::io::Error::last_os_error());
    unsafe {
        let mut sll: libc::sockaddr_ll = std::mem::zeroed();
        sll.sll_family = libc::AF_PACKET as u16;
        sll.sll_protocol = ETH_P_ALL.to_be();
        sll.sll_ifindex = ifindex;
        assert_eq!(libc::bind(fd, &sll as *const _ as *const libc::sockaddr, std::mem::size_of::<libc::sockaddr_ll>() as u32), 0);
        let mut mr: libc::packet_mreq = std::mem::zeroed();
        mr.mr_ifindex = ifindex;
        mr.mr_type = libc::PACKET_MR_PROMISC as u16;
        assert_eq!(libc::setsockopt(fd, libc::SOL_PACKET, libc::PACKET_ADD_MEMBERSHIP,
                                    &mr as *const _ as *const libc::c_void, std::mem::size_of::<libc::packet_mreq>() as u32), 0,
                   "promisc: {}", std::io::Error::last_os_error());
        let one: i32 = 1;
        libc::setsockopt(fd, libc::SOL_PACKET, 23 /* PACKET_IGNORE_OUTGOING */, &one as *const _ as *const libc::c_void, 4);
        let buf: i32 = 256 << 20;
        libc::setsockopt(fd, libc::SOL_SOCKET, libc::SO_RCVBUFFORCE, &buf as *const _ as *const libc::c_void, 4);
        let tv = libc::timeval { tv_sec: 0, tv_usec: 200_000 };
        libc::setsockopt(fd, libc::SOL_SOCKET, libc::SO_RCVTIMEO, &tv as *const _ as *const libc::c_void,
                         std::mem::size_of::<libc::timeval>() as u32);
    }
    let send = |f: &[u8]| unsafe {
        let mut sll: libc::sockaddr_ll = std::mem::zeroed();
        sll.sll_family = libc::AF_PACKET as u16;
        sll.sll_ifindex = ifindex;
        sll.sll_halen = 6;
        sll.sll_addr[..6].copy_from_slice(&BOARD_MAC);
        let n = libc::sendto(fd, f.as_ptr() as *const libc::c_void, f.len(), 0,
                             &sll as *const _ as *const libc::sockaddr, std::mem::size_of::<libc::sockaddr_ll>() as u32);
        assert_eq!(n as usize, f.len(), "send: {}", std::io::Error::last_os_error());
    };
    let h = Header { version: VERSION, ftype: FrameType::Control, length: 0, seq: 0, ts: 0 };
    send(&encode(BOARD_MAC, my_mac, &h, &Body::Control { cmd: CMD_TPUT_TX, arg0: count, arg1: 1500 }, 60));

    let t0 = Instant::now();
    let mut v: Vec<Rx> = Vec::with_capacity(count as usize + 10_000);
    let mut buf = vec![0u8; 16384];
    let mut idle = Instant::now();
    while t0.elapsed() < Duration::from_secs(10) && idle.elapsed() < Duration::from_millis(500) {
        let n = unsafe { libc::recv(fd, buf.as_mut_ptr() as *mut libc::c_void, buf.len(), libc::MSG_TRUNC) };
        if n < 0 {
            continue;
        }
        idle = Instant::now();
        let n = n as usize;
        let f = &buf[..n.min(buf.len())];
        if f.len() < 14 {
            v.push(Rx { t_ns: t0.elapsed().as_nanos(), len: n, dst: [0; 6], ethertype: 0, kind: "runt".into(), seq: None });
            continue;
        }
        let dst: [u8; 6] = f[0..6].try_into().unwrap();
        let et = u16::from_be_bytes([f[12], f[13]]);
        let (kind, seq) = match decode(f) {
            Ok((h, _, _)) => (format!("{:?}", h.ftype), Some(h.seq)),
            Err(e) => (format!("bad:{e:?}"), None),
        };
        v.push(Rx { t_ns: t0.elapsed().as_nanos(), len: n, dst, ethertype: et, kind, seq });
    }
    let mut st: libc::tpacket_stats = unsafe { std::mem::zeroed() };
    let mut sl = std::mem::size_of::<libc::tpacket_stats>() as u32;
    unsafe { libc::getsockopt(fd, libc::SOL_PACKET, libc::PACKET_STATISTICS, &mut st as *mut _ as *mut libc::c_void, &mut sl) };

    let tput: Vec<usize> = (0..v.len()).filter(|&i| v[i].kind == "TputData").collect();
    let audio: Vec<usize> = (0..v.len()).filter(|&i| v[i].kind == "AudioOut").collect();
    let odd: Vec<usize> = (0..v.len())
        .filter(|&i| !(v[i].dst == my_mac && v[i].ethertype == ETHERTYPE && !v[i].kind.starts_with("bad")))
        .collect();
    println!("frames {} (tput {}, audio {}, other/odd {}), socket drops {}", v.len(), tput.len(), audio.len(), odd.len(), st.tp_drops);
    for &i in odd.iter().take(20) {
        let r = &v[i];
        println!("  odd #{i} t={:.6}s len {} dst {:02x?} type {:04x} {}", r.t_ns as f64 / 1e9, r.len, r.dst, r.ethertype, r.kind);
    }
    let show = |i: usize| {
        let r = &v[i];
        format!("#{i} {:.6}s {} seq {:?} len {}", r.t_ns as f64 / 1e9, r.kind, r.seq, r.len)
    };
    for (name, idx) in [("TPUT", &tput), ("AUDIO_OUT", &audio)] {
        let mut gaps = 0;
        for w in idx.windows(2) {
            let (a, b) = (v[w[0]].seq.unwrap(), v[w[1]].seq.unwrap());
            if b != a.wrapping_add(1) {
                gaps += 1;
                if gaps <= 10 {
                    println!("{name} gap: seq {a} -> {b}");
                    for i in w[0].saturating_sub(3)..(w[1] + 3).min(v.len()) {
                        println!("    {}", show(i));
                    }
                }
            }
        }
        println!("{name}: {} frames, {gaps} gaps", idx.len());
    }
    if let (Some(&a), Some(&b)) = (tput.first(), tput.last()) {
        println!("TPUT first seq {:?}, last seq {:?}, expected 0..{}", v[a].seq, v[b].seq, count - 1);
    }
}
