// SPDX-License-Identifier: BSD-3-Clause
// Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/

//! Test-only [`ToolCaller`] that answers from a closure and records calls.

use crate::mcp::{McpError, ToolCaller};
use serde_json::{Value, json};

/// `get_serial_decoder_list` reply with our decoder, id "103".
pub fn list_reply() -> Value {
    json!({"result":[{"id":"103","protocol":"I2C","enabled":true,
        "channels":[{"role":"Data","channel":"D1"},{"role":"Clock","channel":"D0"}]}],
        "status":"Success"})
}

/// `groupBy:"Data"` summary with these keys.
pub fn data_reply(keys: &[&str]) -> Value {
    let groups: Vec<Value> = keys.iter().map(|k| json!({"key":k,"count":1,"errorCount":0})).collect();
    json!({"result":{"id":"103","groupBy":"Data","groups":groups},"status":"Success"})
}

/// `groupBy:"Address"` summary with these keys.
pub fn addr_reply(keys: &[&str]) -> Value {
    let groups: Vec<Value> = keys.iter().map(|k| json!({"key":k,"count":1})).collect();
    json!({"result":{"id":"103","groupBy":"Address","groups":groups},"status":"Success"})
}

type Handler = Box<dyn FnMut(&str, &Value) -> Result<Value, McpError>>;

pub struct FakeCaller {
    handler: Handler,
    pub calls: Vec<(String, Value)>,
}

impl FakeCaller {
    pub fn new(handler: impl FnMut(&str, &Value) -> Result<Value, McpError> + 'static) -> Self {
        FakeCaller { handler: Box::new(handler), calls: Vec::new() }
    }

    /// Number of calls to `name`.
    pub fn count(&self, name: &str) -> usize {
        self.calls.iter().filter(|(n, _)| n == name).count()
    }
}

impl ToolCaller for FakeCaller {
    fn call_tool(&mut self, name: &str, args: Value) -> Result<Value, McpError> {
        self.calls.push((name.to_string(), args.clone()));
        (self.handler)(name, &args)
    }
}
