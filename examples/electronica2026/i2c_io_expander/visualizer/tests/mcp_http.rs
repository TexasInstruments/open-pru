// SPDX-License-Identifier: BSD-3-Clause
// Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/

//! McpClient against a fake MCP server on a local TCP port.

use serde_json::{Value, json};
use std::io::{BufRead, BufReader, Read, Write};
use std::net::TcpListener;
use std::sync::{Arc, Mutex};
use std::thread;
use std::time::Duration;
use viz::{McpClient, McpError, ToolCaller};

/// One request as the fake server saw it.
#[derive(Clone, Debug)]
struct Req {
    method: String,
    session: Option<String>,
}

/// What the fake server sends back.
struct Resp {
    status: u16,
    headers: Vec<(&'static str, String)>,
    body: String,
    delay: Duration,
}

impl Resp {
    fn json(body: Value) -> Resp {
        Resp {
            status: 200,
            headers: vec![("Content-Type", "application/json".into())],
            body: body.to_string(),
            delay: Duration::ZERO,
        }
    }
    fn status(status: u16) -> Resp {
        Resp { status, headers: vec![], body: String::new(), delay: Duration::ZERO }
    }
    fn session(mut self, id: &str) -> Resp {
        self.headers.push(("Mcp-Session-Id", id.into()));
        self
    }
    fn delayed(mut self, d: Duration) -> Resp {
        self.delay = d;
        self
    }
}

/// Start a fake server. `handler` gets each request and all earlier ones.
fn fake_server(
    handler: impl Fn(&Req, &[Req]) -> Resp + Send + 'static,
) -> (String, Arc<Mutex<Vec<Req>>>) {
    let listener = TcpListener::bind("127.0.0.1:0").unwrap();
    let url = format!("http://{}/mcp", listener.local_addr().unwrap());
    let log = Arc::new(Mutex::new(Vec::<Req>::new()));
    let log2 = log.clone();
    thread::spawn(move || {
        for stream in listener.incoming() {
            let Ok(mut stream) = stream else { continue };
            let mut reader = BufReader::new(stream.try_clone().unwrap());
            let mut len = 0usize;
            let mut session = None;
            loop {
                let mut line = String::new();
                if reader.read_line(&mut line).unwrap_or(0) == 0 {
                    break;
                }
                let line = line.trim_end();
                if line.is_empty() {
                    break;
                }
                if let Some((k, v)) = line.split_once(':') {
                    let k = k.trim().to_ascii_lowercase();
                    if k == "content-length" {
                        len = v.trim().parse().unwrap();
                    } else if k == "mcp-session-id" {
                        session = Some(v.trim().to_string());
                    }
                }
            }
            let mut body = vec![0u8; len];
            reader.read_exact(&mut body).unwrap();
            let v: Value = serde_json::from_slice(&body).unwrap_or(Value::Null);
            let req = Req { method: v["method"].as_str().unwrap_or("").to_string(), session };
            let earlier = log2.lock().unwrap().clone();
            let resp = handler(&req, &earlier);
            log2.lock().unwrap().push(req);
            thread::sleep(resp.delay);
            let mut out = format!(
                "HTTP/1.1 {} X\r\nContent-Length: {}\r\nConnection: close\r\n",
                resp.status,
                resp.body.len()
            );
            for (k, v) in &resp.headers {
                out.push_str(&format!("{k}: {v}\r\n"));
            }
            out.push_str("\r\n");
            out.push_str(&resp.body);
            let _ = stream.write_all(out.as_bytes());
        }
    });
    (url, log)
}

fn tool_reply(inner: Value) -> Value {
    json!({"jsonrpc":"2.0","id":2,"result":{"content":[{"type":"text","text":inner.to_string()}]}})
}

fn init_reply() -> Value {
    json!({"jsonrpc":"2.0","id":1,"result":{"protocolVersion":"2025-06-18","capabilities":{},"serverInfo":{"name":"fake","version":"0"}}})
}

/// Standard handler: initialize gives session `s1`, tools/call answers `inner`.
fn simple(inner: Value) -> impl Fn(&Req, &[Req]) -> Resp {
    move |r, _| match r.method.as_str() {
        "initialize" => Resp::json(init_reply()).session("s1"),
        "notifications/initialized" => Resp::status(202),
        _ => Resp::json(tool_reply(inner.clone())),
    }
}

const T3: Duration = Duration::from_secs(3);

#[test]
fn initializes_then_calls_tool() {
    let (url, log) = fake_server(simple(json!({"status":"Success","result":{"x":1}})));
    let mut c = McpClient::new(&url, T3);
    let v = c.call_tool("get_serial_decoder_list", json!({})).unwrap();
    assert_eq!(v["result"]["x"], 1);
    let log = log.lock().unwrap();
    let methods: Vec<_> = log.iter().map(|r| r.method.as_str()).collect();
    assert_eq!(methods, ["initialize", "notifications/initialized", "tools/call"]);
    assert_eq!(log[2].session.as_deref(), Some("s1"));
}

#[test]
fn parses_sse_response() {
    let (url, _) = fake_server(|r, _| match r.method.as_str() {
        "initialize" => Resp::json(init_reply()).session("s1"),
        "notifications/initialized" => Resp::status(202),
        _ => {
            let data = tool_reply(json!({"status":"Success","result":{"x":1}}));
            Resp {
                status: 200,
                headers: vec![("Content-Type", "text/event-stream".into())],
                body: format!("event: message\ndata: {data}\n\n"),
                delay: Duration::ZERO,
            }
        }
    });
    let mut c = McpClient::new(&url, T3);
    let v = c.call_tool("x", json!({})).unwrap();
    assert_eq!(v["result"]["x"], 1);
}

#[test]
fn reinitializes_after_session_rejected() {
    let (url, log) = fake_server(|r, earlier| {
        let inits = earlier.iter().filter(|e| e.method == "initialize").count();
        match r.method.as_str() {
            "initialize" => Resp::json(init_reply()).session(if inits == 0 { "s1" } else { "s2" }),
            "notifications/initialized" => Resp::status(202),
            _ if r.session.as_deref() == Some("s1") => Resp::status(404),
            _ => Resp::json(tool_reply(json!({"status":"Success","result":{"x":2}}))),
        }
    });
    let mut c = McpClient::new(&url, T3);
    let v = c.call_tool("x", json!({})).unwrap();
    assert_eq!(v["result"]["x"], 2);
    let log = log.lock().unwrap();
    let last = log.last().unwrap();
    assert_eq!(last.method, "tools/call");
    assert_eq!(last.session.as_deref(), Some("s2"));
}

#[test]
fn slow_reply_under_timeout_ok() {
    let (url, _) = fake_server(|r, _| match r.method.as_str() {
        "initialize" => Resp::json(init_reply()).session("s1"),
        "notifications/initialized" => Resp::status(202),
        _ => Resp::json(tool_reply(json!({"status":"Success"}))).delayed(Duration::from_secs(1)),
    });
    let mut c = McpClient::new(&url, T3);
    assert!(c.call_tool("x", json!({})).is_ok());
}

#[test]
fn reply_over_timeout_is_error() {
    let (url, _) = fake_server(|r, _| match r.method.as_str() {
        "initialize" => Resp::json(init_reply()).session("s1"),
        "notifications/initialized" => Resp::status(202),
        _ => Resp::json(tool_reply(json!({"status":"Success"}))).delayed(Duration::from_secs(4)),
    });
    let mut c = McpClient::new(&url, T3);
    let r = c.call_tool("x", json!({}));
    assert!(matches!(r, Err(McpError::Timeout)), "{r:?}");
}

#[test]
fn reinitializes_after_timeout() {
    // PicoScope 7 restarted: the old session may be answered with anything,
    // so a lost connection must force a fresh initialize on the next call.
    let (url, log) = fake_server(|r, earlier| {
        let calls = earlier.iter().filter(|e| e.method == "tools/call").count();
        match r.method.as_str() {
            "initialize" => Resp::json(init_reply()).session("s1"),
            "notifications/initialized" => Resp::status(202),
            _ if calls == 1 => Resp::json(tool_reply(json!({"status":"Success"}))).delayed(Duration::from_secs(1)),
            _ => Resp::json(tool_reply(json!({"status":"Success"}))),
        }
    });
    let mut c = McpClient::new(&url, Duration::from_millis(300));
    assert!(c.call_tool("x", json!({})).is_ok());
    assert!(matches!(c.call_tool("x", json!({})), Err(McpError::Timeout)));
    // The fake serves one request at a time: let the stalled reply finish.
    thread::sleep(Duration::from_millis(900));
    assert!(c.call_tool("x", json!({})).is_ok());
    let log = log.lock().unwrap();
    let methods: Vec<_> = log.iter().map(|r| r.method.as_str()).collect();
    assert_eq!(
        methods,
        ["initialize", "notifications/initialized", "tools/call", "tools/call",
         "initialize", "notifications/initialized", "tools/call"]
    );
}

#[test]
fn connection_refused_is_connect_error() {
    let port = TcpListener::bind("127.0.0.1:0").unwrap().local_addr().unwrap().port();
    let mut c = McpClient::new(&format!("http://127.0.0.1:{port}/mcp"), T3);
    let r = c.call_tool("x", json!({}));
    assert!(matches!(r, Err(McpError::Connect(_))), "{r:?}");
}

#[test]
fn jsonrpc_error_is_protocol_error() {
    let (url, _) = fake_server(|r, _| match r.method.as_str() {
        "initialize" => Resp::json(init_reply()).session("s1"),
        "notifications/initialized" => Resp::status(202),
        _ => Resp::json(json!({"jsonrpc":"2.0","id":2,"error":{"code":-32602,"message":"bad"}})),
    });
    let mut c = McpClient::new(&url, T3);
    let r = c.call_tool("x", json!({}));
    assert!(matches!(r, Err(McpError::Protocol(_))), "{r:?}");
}

/// Needs PicoScope 7 running with its MCP server on (port 18123 or
/// `PICO_MCP_URL`). Run with `cargo test --test mcp_http -- --ignored`.
#[test]
#[ignore]
fn real_picoscope_lists_decoders() {
    let url = std::env::var("PICO_MCP_URL").unwrap_or("http://127.0.0.1:18123/mcp".into());
    let mut c = McpClient::new(&url, T3);
    let v = c.call_tool("get_serial_decoder_list", json!({})).unwrap();
    assert_eq!(v["status"], "Success", "{v}");
}
