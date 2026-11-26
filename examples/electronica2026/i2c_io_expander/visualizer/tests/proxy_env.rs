// SPDX-License-Identifier: BSD-3-Clause
// Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/

//! PicoScope 7 is on localhost: proxy environment variables must not apply.
//! Own test binary, because it sets process-wide environment variables.

use serde_json::json;
use std::io::{BufRead, BufReader, Read, Write};
use std::net::TcpListener;
use std::thread;
use std::time::Duration;
use viz::{McpClient, ToolCaller};

#[test]
fn ignores_proxy_env_for_picoscope() {
    // A proxy that refuses every connection.
    let dead = TcpListener::bind("127.0.0.1:0").unwrap().local_addr().unwrap().port();
    let proxy = format!("http://127.0.0.1:{dead}");
    // SAFETY: only test in this binary; no other thread reads the environment yet.
    unsafe {
        for k in ["http_proxy", "HTTP_PROXY", "https_proxy", "HTTPS_PROXY", "all_proxy", "ALL_PROXY"] {
            std::env::set_var(k, &proxy);
        }
        std::env::remove_var("no_proxy");
        std::env::remove_var("NO_PROXY");
    }

    let listener = TcpListener::bind("127.0.0.1:0").unwrap();
    let url = format!("http://{}/mcp", listener.local_addr().unwrap());
    thread::spawn(move || {
        for (n, stream) in listener.incoming().enumerate() {
            let mut stream = stream.unwrap();
            let mut reader = BufReader::new(stream.try_clone().unwrap());
            let mut len = 0;
            loop {
                let mut line = String::new();
                reader.read_line(&mut line).unwrap();
                if line.trim().is_empty() {
                    break;
                }
                if let Some(v) = line.to_ascii_lowercase().strip_prefix("content-length:") {
                    len = v.trim().parse().unwrap();
                }
            }
            let mut body = vec![0; len];
            reader.read_exact(&mut body).unwrap();
            let reply = match n {
                0 => json!({"jsonrpc":"2.0","id":1,"result":{}}).to_string(),
                1 => String::new(),
                _ => json!({"jsonrpc":"2.0","id":2,"result":{"content":[{"type":"text","text":"{\"status\":\"Success\"}"}]}}).to_string(),
            };
            let out = format!("HTTP/1.1 200 OK\r\nContent-Length: {}\r\nConnection: close\r\n\r\n{reply}", reply.len());
            stream.write_all(out.as_bytes()).unwrap();
        }
    });

    let mut c = McpClient::new(&url, Duration::from_secs(3));
    let v = c.call_tool("x", json!({})).expect("must reach localhost directly, not via the proxy");
    assert_eq!(v["status"], "Success");
}
