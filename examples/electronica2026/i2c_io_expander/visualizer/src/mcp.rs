// SPDX-License-Identifier: BSD-3-Clause
// Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/

//! Minimal MCP client (streamable HTTP, JSON-RPC 2.0) for PicoScope 7's
//! built-in MCP server.

use serde_json::Value;
use std::fmt;
use std::time::Duration;

/// Something that can call an MCP tool. Implemented by [`McpClient`]; tests
/// use fakes.
pub trait ToolCaller {
    /// Call `name` with `args`. Returns the tool's own JSON result (the
    /// parsed `result.content[0].text`), e.g. `{"status":"Success",...}`.
    fn call_tool(&mut self, name: &str, args: Value) -> Result<Value, McpError>;
}

#[derive(Debug)]
pub enum McpError {
    /// Could not connect (PicoScope 7 not running, MCP off, wrong port).
    Connect(String),
    /// No reply within the timeout.
    Timeout,
    /// HTTP status other than 2xx.
    Http(u16),
    /// Reply was not the expected JSON-RPC / MCP shape, or a JSON-RPC error.
    Protocol(String),
}

impl fmt::Display for McpError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            McpError::Connect(e) => write!(f, "connect: {e}"),
            McpError::Timeout => write!(f, "timeout"),
            McpError::Http(c) => write!(f, "HTTP {c}"),
            McpError::Protocol(e) => write!(f, "protocol: {e}"),
        }
    }
}

impl std::error::Error for McpError {}

/// MCP protocol version sent in `initialize`.
const PROTOCOL_VERSION: &str = "2025-06-18";

/// Blocking MCP client. Initializes lazily on the first call and again
/// whenever PicoScope 7 rejects the session (HTTP 400/404, e.g. after a
/// PicoScope restart).
pub struct McpClient {
    url: String,
    agent: ureq::Agent,
    session: Option<String>,
    initialized: bool,
    next_id: u64,
}

impl McpClient {
    pub fn new(url: &str, timeout: Duration) -> Self {
        let agent = ureq::Agent::config_builder()
            .timeout_global(Some(timeout))
            .http_status_as_error(false)
            .build()
            .into();
        McpClient { url: url.to_string(), agent, session: None, initialized: false, next_id: 0 }
    }

    /// POST one JSON-RPC message. Returns the parsed reply, or `None` for
    /// an empty body (notifications).
    fn post(&mut self, msg: &Value) -> Result<Option<Value>, McpError> {
        let mut req = self
            .agent
            .post(&self.url)
            .header("Content-Type", "application/json")
            .header("Accept", "application/json, text/event-stream");
        if let Some(s) = &self.session {
            req = req.header("Mcp-Session-Id", s);
        }
        let mut resp = req.send(msg.to_string()).map_err(map_err)?;
        let status = resp.status().as_u16();
        if !(200..300).contains(&status) {
            return Err(McpError::Http(status));
        }
        if let Some(s) = resp.headers().get("mcp-session-id").and_then(|v| v.to_str().ok()) {
            self.session = Some(s.to_string());
        }
        let body = resp.body_mut().read_to_string().map_err(map_err)?;
        parse_body(&body)
    }

    fn request(&mut self, method: &str, params: Value) -> Result<Value, McpError> {
        self.next_id += 1;
        let msg = serde_json::json!({"jsonrpc":"2.0","id":self.next_id,"method":method,"params":params});
        let reply = self
            .post(&msg)?
            .ok_or_else(|| McpError::Protocol(format!("empty reply to {method}")))?;
        if let Some(e) = reply.get("error") {
            return Err(McpError::Protocol(e.to_string()));
        }
        reply
            .get("result")
            .cloned()
            .ok_or_else(|| McpError::Protocol(format!("no result in reply to {method}")))
    }

    fn initialize(&mut self) -> Result<(), McpError> {
        self.session = None;
        self.initialized = false;
        self.request(
            "initialize",
            serde_json::json!({
                "protocolVersion": PROTOCOL_VERSION,
                "capabilities": {},
                "clientInfo": {"name": "i2c_led_visualizer", "version": env!("CARGO_PKG_VERSION")}
            }),
        )?;
        self.post(&serde_json::json!({"jsonrpc":"2.0","method":"notifications/initialized"}))?;
        self.initialized = true;
        Ok(())
    }

    fn call_once(&mut self, name: &str, args: &Value) -> Result<Value, McpError> {
        let result = self.request("tools/call", serde_json::json!({"name": name, "arguments": args}))?;
        let text = result["content"][0]["text"]
            .as_str()
            .ok_or_else(|| McpError::Protocol(format!("no text content from {name}")))?;
        serde_json::from_str(text).map_err(|e| McpError::Protocol(format!("{name}: {e}")))
    }
}

/// Body is either plain JSON or SSE; for SSE take the last `data:` line.
fn parse_body(body: &str) -> Result<Option<Value>, McpError> {
    let json = match body.lines().filter_map(|l| l.strip_prefix("data:")).last() {
        Some(data) => data.trim(),
        None => body.trim(),
    };
    if json.is_empty() {
        return Ok(None);
    }
    serde_json::from_str(json).map(Some).map_err(|e| McpError::Protocol(e.to_string()))
}

fn map_err(e: ureq::Error) -> McpError {
    match e {
        ureq::Error::Timeout(_) => McpError::Timeout,
        ureq::Error::StatusCode(c) => McpError::Http(c),
        other => McpError::Connect(other.to_string()),
    }
}

impl ToolCaller for McpClient {
    fn call_tool(&mut self, name: &str, args: Value) -> Result<Value, McpError> {
        if !self.initialized {
            self.initialize()?;
        }
        match self.call_once(name, &args) {
            Err(McpError::Http(400 | 404)) => {
                self.initialize()?;
                self.call_once(name, &args)
            }
            r => r,
        }
    }
}
