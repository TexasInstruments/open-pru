// SPDX-License-Identifier: BSD-3-Clause
// Copyright (C) 2026 Texas Instruments Incorporated - http://www.ti.com/

//! Find, enable or add PicoScope 7's I2C decoder on D1 (SDA) / D0 (SCL).

use crate::mcp::{McpError, ToolCaller};
use serde_json::{Value, json};

/// Check a tool reply's `status` field.
pub(crate) fn require_success(tool: &str, v: &Value) -> Result<(), McpError> {
    match v["status"].as_str() {
        Some("Success") => Ok(()),
        _ => Err(McpError::Protocol(format!("{tool}: {}", v["message"].as_str().unwrap_or("not Success")))),
    }
}

/// True if decoder `d` is I2C with Data on D1 and Clock on D0.
fn is_our_decoder(d: &Value) -> bool {
    let on = |role: &str, ch: &str| {
        d["channels"]
            .as_array()
            .is_some_and(|cs| cs.iter().any(|c| c["role"] == role && c["channel"] == ch))
    };
    d["protocol"] == "I2C" && on("Data", "D1") && on("Clock", "D0")
}

fn id_of(d: &Value) -> Option<String> {
    d["id"].as_str().map(str::to_string)
}

/// Return the id of the I2C decoder to read (design.md, Behaviour 10):
/// the first enabled I2C decoder on D1/D0; else enable a disabled one;
/// else add one. Other decoders and settings are never touched.
pub fn ensure_decoder(c: &mut dyn ToolCaller) -> Result<String, McpError> {
    let list = c.call_tool("get_serial_decoder_list", json!({}))?;
    require_success("get_serial_decoder_list", &list)?;
    let ours: Vec<&Value> = list["result"].as_array().into_iter().flatten().filter(|d| is_our_decoder(d)).collect();

    if let Some(id) = ours.iter().find(|d| d["enabled"] == true).and_then(|d| id_of(d)) {
        return Ok(id);
    }
    if let Some(id) = ours.first().and_then(|d| id_of(d)) {
        let r = c.call_tool("configure_serial_decoder", json!({"action":"Configure","id":id,"enabled":true}))?;
        require_success("configure_serial_decoder", &r)?;
        return Ok(id);
    }
    let r = c.call_tool(
        "configure_serial_decoder",
        json!({"action":"Add","protocol":"I2C","name":"TCA9538",
               "channels":{"Data":"D1","Clock":"D0"},
               "settings":{"Bus Speed":"Fast-mode Plus (1 Mbit/s)"},"enabled":true}),
    )?;
    require_success("configure_serial_decoder", &r)?;
    r["decoderId"]
        .as_str()
        .map(str::to_string)
        .ok_or_else(|| McpError::Protocol("configure_serial_decoder: no decoderId".into()))
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::fake::FakeCaller;
    use serde_json::{Value, json};

    fn decoder(id: &str, enabled: bool, data: &str, clock: &str) -> Value {
        json!({"id":id,"protocol":"I2C","name":"x","enabled":enabled,
               "channels":[{"role":"Data","channel":data},{"role":"Clock","channel":clock}],
               "settings":{"Bus Speed":"Fast-mode Plus (1 Mbit/s)"}})
    }

    fn with_list(list: Vec<Value>) -> FakeCaller {
        FakeCaller::new(move |name, _| match name {
            "get_serial_decoder_list" => Ok(json!({"result": list.clone(), "status":"Success"})),
            "configure_serial_decoder" => Ok(json!({"status":"Success","decoderId":"103","stateChanged":true})),
            other => panic!("unexpected tool {other}"),
        })
    }

    #[test]
    fn ensure_uses_enabled_match() {
        let mut c = with_list(vec![decoder("7", false, "D1", "D0"), decoder("9", true, "D1", "D0")]);
        assert_eq!(ensure_decoder(&mut c).unwrap(), "9");
        assert_eq!(c.count("configure_serial_decoder"), 0);
    }

    #[test]
    fn ensure_enables_disabled_match() {
        let mut c = with_list(vec![decoder("7", false, "D1", "D0")]);
        assert_eq!(ensure_decoder(&mut c).unwrap(), "7");
        assert_eq!(c.count("configure_serial_decoder"), 1);
        assert_eq!(c.calls[1].1, json!({"action":"Configure","id":"7","enabled":true}));
    }

    #[test]
    fn ensure_adds_when_missing() {
        let mut c = with_list(vec![]);
        assert_eq!(ensure_decoder(&mut c).unwrap(), "103");
        assert_eq!(
            c.calls[1].1,
            json!({"action":"Add","protocol":"I2C","name":"TCA9538",
                   "channels":{"Data":"D1","Clock":"D0"},
                   "settings":{"Bus Speed":"Fast-mode Plus (1 Mbit/s)"},"enabled":true})
        );
    }

    #[test]
    fn ensure_ignores_other_channels() {
        let mut c = with_list(vec![decoder("5", true, "D3", "D2")]);
        assert_eq!(ensure_decoder(&mut c).unwrap(), "103");
        assert_eq!(c.calls[1].1["action"], "Add");
    }

    #[test]
    fn ensure_list_rejected_is_error() {
        let mut c = FakeCaller::new(|_, _| Ok(json!({"status":"Rejected","message":"no device"})));
        assert!(ensure_decoder(&mut c).is_err());
    }
}
