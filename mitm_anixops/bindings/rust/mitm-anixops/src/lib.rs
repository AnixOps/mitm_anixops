use std::ffi::{CStr, CString};
use std::fmt;
use std::os::raw::{c_char, c_int};

const ANIXOPS_OK: c_int = 0;
const ANIXOPS_VALUE_CAP: usize = 2048;
const ANIXOPS_ARGUMENT_CAP: usize = 4096;
const ANIXOPS_MESSAGE_CAP: usize = 256;
const ANIXOPS_PATTERN_CAP: usize = 256;
const ANIXOPS_PLAN_HEADER_CAP: usize = 16;

#[repr(C)]
struct AnixopsEngine {
    _private: [u8; 0],
}

#[repr(C)]
#[derive(Clone, Copy)]
struct AnixopsRewriteResult {
    action: c_int,
    status_code: c_int,
    rule_index: c_int,
    matched_pattern: [c_char; ANIXOPS_PATTERN_CAP],
    value: [c_char; ANIXOPS_VALUE_CAP],
    message: [c_char; ANIXOPS_MESSAGE_CAP],
}

#[repr(C)]
#[derive(Clone, Copy)]
struct AnixopsHeaderRewriteResult {
    action: c_int,
    phase: c_int,
    rule_index: c_int,
    matched_pattern: [c_char; ANIXOPS_PATTERN_CAP],
    header_name: [c_char; ANIXOPS_PATTERN_CAP],
    value: [c_char; ANIXOPS_VALUE_CAP],
    message: [c_char; ANIXOPS_MESSAGE_CAP],
}

#[repr(C)]
#[derive(Clone, Copy)]
struct AnixopsScriptResult {
    kind: c_int,
    phase: c_int,
    requires_body: c_int,
    rule_index: c_int,
    matched_pattern: [c_char; ANIXOPS_PATTERN_CAP],
    script_path: [c_char; ANIXOPS_VALUE_CAP],
    tag: [c_char; ANIXOPS_VALUE_CAP],
    argument: [c_char; ANIXOPS_ARGUMENT_CAP],
    message: [c_char; ANIXOPS_MESSAGE_CAP],
}

#[repr(C)]
#[derive(Clone, Copy)]
struct AnixopsRewritePlan {
    phase: c_int,
    body_available: c_int,
    requires_body: c_int,
    rewrite: AnixopsRewriteResult,
    header_rewrite_count: usize,
    header_rewrite_truncated: c_int,
    header_rewrites: [AnixopsHeaderRewriteResult; ANIXOPS_PLAN_HEADER_CAP],
    script: AnixopsScriptResult,
}

extern "C" {
    fn anixops_version() -> *const c_char;
    fn anixops_status_message(status: c_int) -> *const c_char;
    fn anixops_engine_new() -> *mut AnixopsEngine;
    fn anixops_engine_free(engine: *mut AnixopsEngine);
    fn anixops_engine_load_config(engine: *mut AnixopsEngine, config_text: *const c_char) -> c_int;
    fn anixops_engine_rewrite_rule_count(engine: *const AnixopsEngine) -> usize;
    fn anixops_engine_script_rule_count(engine: *const AnixopsEngine) -> usize;
    fn anixops_rewrite_evaluate_url(
        engine: *const AnixopsEngine,
        url: *const c_char,
        phase: c_int,
        out_result: *mut AnixopsRewriteResult,
    ) -> c_int;
    fn anixops_rewrite_apply_body(
        engine: *const AnixopsEngine,
        url: *const c_char,
        phase: c_int,
        body: *const c_char,
        out_body: *mut c_char,
        out_body_cap: usize,
        out_result: *mut AnixopsRewriteResult,
    ) -> c_int;
    fn anixops_rewrite_evaluate_named_header(
        engine: *const AnixopsEngine,
        url: *const c_char,
        phase: c_int,
        start_index: usize,
        header_name: *const c_char,
        current_header_value: *const c_char,
        out_result: *mut AnixopsHeaderRewriteResult,
    ) -> c_int;
    fn anixops_script_evaluate_url(
        engine: *const AnixopsEngine,
        url: *const c_char,
        phase: c_int,
        out_result: *mut AnixopsScriptResult,
    ) -> c_int;
    fn anixops_rewrite_build_plan(
        engine: *const AnixopsEngine,
        url: *const c_char,
        phase: c_int,
        body: *const c_char,
        out_body: *mut c_char,
        out_body_cap: usize,
        current_header_value: *const c_char,
        out_plan: *mut AnixopsRewritePlan,
    ) -> c_int;
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum Phase {
    Request,
    Response,
}

impl Phase {
    fn as_c(self) -> c_int {
        match self {
            Phase::Request => 0,
            Phase::Response => 1,
        }
    }
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum RewriteAction {
    None,
    Redirect302,
    ResponseBodyReplaceRegex,
    HeaderAdd,
    HeaderReplace,
    ResponseHeaderAdd,
    Other(i32),
}

impl From<c_int> for RewriteAction {
    fn from(value: c_int) -> Self {
        match value {
            0 => RewriteAction::None,
            1 => RewriteAction::Redirect302,
            12 => RewriteAction::ResponseBodyReplaceRegex,
            14 => RewriteAction::HeaderAdd,
            13 => RewriteAction::HeaderReplace,
            18 => RewriteAction::ResponseHeaderAdd,
            other => RewriteAction::Other(other),
        }
    }
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum ScriptKind {
    None,
    HttpRequest,
    HttpResponse,
    Other(i32),
}

impl From<c_int> for ScriptKind {
    fn from(value: c_int) -> Self {
        match value {
            0 => ScriptKind::None,
            1 => ScriptKind::HttpRequest,
            2 => ScriptKind::HttpResponse,
            other => ScriptKind::Other(other),
        }
    }
}

#[derive(Debug)]
pub struct Error {
    operation: &'static str,
    status: i32,
    message: String,
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{} failed: {} ({})", self.operation, self.message, self.status)
    }
}

impl std::error::Error for Error {}

pub struct Engine {
    ptr: *mut AnixopsEngine,
}

#[derive(Debug, Eq, PartialEq)]
pub struct RewriteResult {
    pub action: RewriteAction,
    pub status_code: i32,
    pub rule_index: i32,
    pub matched_pattern: String,
    pub value: String,
    pub message: String,
}

#[derive(Debug, Eq, PartialEq)]
pub struct HeaderRewriteResult {
    pub action: RewriteAction,
    pub phase: Phase,
    pub rule_index: i32,
    pub matched_pattern: String,
    pub header_name: String,
    pub value: String,
    pub message: String,
}

#[derive(Debug, Eq, PartialEq)]
pub struct ScriptResult {
    pub kind: ScriptKind,
    pub phase: Phase,
    pub requires_body: bool,
    pub rule_index: i32,
    pub matched_pattern: String,
    pub script_path: String,
    pub tag: String,
    pub argument: String,
    pub message: String,
}

#[derive(Debug, Eq, PartialEq)]
pub struct RewritePlan {
    pub phase: Phase,
    pub body_available: bool,
    pub requires_body: bool,
    pub rewrite: RewriteResult,
    pub header_rewrites: Vec<HeaderRewriteResult>,
    pub header_rewrite_truncated: bool,
    pub script: ScriptResult,
}

impl Engine {
    pub fn new() -> Result<Self, Error> {
        let ptr = unsafe { anixops_engine_new() };
        if ptr.is_null() {
            return Err(Error {
                operation: "engine_new",
                status: -2,
                message: "out of memory".to_string(),
            });
        }
        Ok(Self { ptr })
    }

    pub fn load_config(&mut self, config: &str) -> Result<(), Error> {
        let config = CString::new(config).expect("config must not contain nul bytes");
        check_status(
            "load_config",
            unsafe { anixops_engine_load_config(self.ptr, config.as_ptr()) },
        )
    }

    pub fn rewrite_rule_count(&self) -> usize {
        unsafe { anixops_engine_rewrite_rule_count(self.ptr) }
    }

    pub fn script_rule_count(&self) -> usize {
        unsafe { anixops_engine_script_rule_count(self.ptr) }
    }

    pub fn evaluate_rewrite(&self, url: &str, phase: Phase) -> Result<RewriteResult, Error> {
        let url = CString::new(url).expect("url must not contain nul bytes");
        let mut result = empty_rewrite_result();
        check_status(
            "evaluate_rewrite",
            unsafe {
                anixops_rewrite_evaluate_url(self.ptr, url.as_ptr(), phase.as_c(), &mut result)
            },
        )?;
        Ok(rewrite_result_from_c(&result))
    }

    pub fn apply_body(
        &self,
        url: &str,
        phase: Phase,
        body: &str,
    ) -> Result<(String, RewriteResult), Error> {
        let url = CString::new(url).expect("url must not contain nul bytes");
        let body_c = CString::new(body).expect("body must not contain nul bytes");
        let mut result = empty_rewrite_result();
        let mut out = vec![0 as c_char; body.len() + ANIXOPS_VALUE_CAP];
        check_status(
            "apply_body",
            unsafe {
                anixops_rewrite_apply_body(
                    self.ptr,
                    url.as_ptr(),
                    phase.as_c(),
                    body_c.as_ptr(),
                    out.as_mut_ptr(),
                    out.len(),
                    &mut result,
                )
            },
        )?;
        Ok((cstr_from_buf(&out), rewrite_result_from_c(&result)))
    }

    pub fn evaluate_script(&self, url: &str, phase: Phase) -> Result<ScriptResult, Error> {
        let url = CString::new(url).expect("url must not contain nul bytes");
        let mut result = empty_script_result();
        check_status(
            "evaluate_script",
            unsafe {
                anixops_script_evaluate_url(self.ptr, url.as_ptr(), phase.as_c(), &mut result)
            },
        )?;
        Ok(script_result_from_c(&result))
    }

    pub fn evaluate_named_header(
        &self,
        url: &str,
        phase: Phase,
        start_index: usize,
        header_name: &str,
        current_header_value: &str,
    ) -> Result<HeaderRewriteResult, Error> {
        let url = CString::new(url).expect("url must not contain nul bytes");
        let header_name = CString::new(header_name).expect("header_name must not contain nul bytes");
        let current_header_value =
            CString::new(current_header_value).expect("current_header_value must not contain nul bytes");
        let mut result = empty_header_rewrite_result();
        check_status(
            "evaluate_named_header",
            unsafe {
                anixops_rewrite_evaluate_named_header(
                    self.ptr,
                    url.as_ptr(),
                    phase.as_c(),
                    start_index,
                    header_name.as_ptr(),
                    current_header_value.as_ptr(),
                    &mut result,
                )
            },
        )?;
        Ok(header_rewrite_result_from_c(&result))
    }

    pub fn build_plan(
        &self,
        url: &str,
        phase: Phase,
        body: &str,
    ) -> Result<(RewritePlan, String), Error> {
        let url = CString::new(url).expect("url must not contain nul bytes");
        let body_c = CString::new(body).expect("body must not contain nul bytes");
        let mut plan = empty_rewrite_plan();
        let mut out = vec![0 as c_char; body.len() + ANIXOPS_VALUE_CAP];
        check_status(
            "build_plan",
            unsafe {
                anixops_rewrite_build_plan(
                    self.ptr,
                    url.as_ptr(),
                    phase.as_c(),
                    body_c.as_ptr(),
                    out.as_mut_ptr(),
                    out.len(),
                    std::ptr::null(),
                    &mut plan,
                )
            },
        )?;
        Ok((rewrite_plan_from_c(&plan), cstr_from_buf(&out)))
    }
}

impl Drop for Engine {
    fn drop(&mut self) {
        if !self.ptr.is_null() {
            unsafe { anixops_engine_free(self.ptr) };
            self.ptr = std::ptr::null_mut();
        }
    }
}

pub fn version() -> String {
    unsafe { CStr::from_ptr(anixops_version()).to_string_lossy().into_owned() }
}

fn check_status(operation: &'static str, status: c_int) -> Result<(), Error> {
    if status == ANIXOPS_OK {
        return Ok(());
    }
    let message = unsafe {
        CStr::from_ptr(anixops_status_message(status))
            .to_string_lossy()
            .into_owned()
    };
    Err(Error {
        operation,
        status,
        message,
    })
}

fn rewrite_result_from_c(result: &AnixopsRewriteResult) -> RewriteResult {
    RewriteResult {
        action: RewriteAction::from(result.action),
        status_code: result.status_code,
        rule_index: result.rule_index,
        matched_pattern: cstr_from_buf(&result.matched_pattern),
        value: cstr_from_buf(&result.value),
        message: cstr_from_buf(&result.message),
    }
}

fn header_rewrite_result_from_c(result: &AnixopsHeaderRewriteResult) -> HeaderRewriteResult {
    let phase = if result.phase == 1 {
        Phase::Response
    } else {
        Phase::Request
    };
    HeaderRewriteResult {
        action: RewriteAction::from(result.action),
        phase,
        rule_index: result.rule_index,
        matched_pattern: cstr_from_buf(&result.matched_pattern),
        header_name: cstr_from_buf(&result.header_name),
        value: cstr_from_buf(&result.value),
        message: cstr_from_buf(&result.message),
    }
}

fn script_result_from_c(result: &AnixopsScriptResult) -> ScriptResult {
    let phase = if result.phase == 1 {
        Phase::Response
    } else {
        Phase::Request
    };
    ScriptResult {
        kind: ScriptKind::from(result.kind),
        phase,
        requires_body: result.requires_body != 0,
        rule_index: result.rule_index,
        matched_pattern: cstr_from_buf(&result.matched_pattern),
        script_path: cstr_from_buf(&result.script_path),
        tag: cstr_from_buf(&result.tag),
        argument: cstr_from_buf(&result.argument),
        message: cstr_from_buf(&result.message),
    }
}

fn rewrite_plan_from_c(plan: &AnixopsRewritePlan) -> RewritePlan {
    let phase = if plan.phase == 1 {
        Phase::Response
    } else {
        Phase::Request
    };
    let count = plan.header_rewrite_count.min(ANIXOPS_PLAN_HEADER_CAP);
    let mut headers = Vec::with_capacity(count);
    for header in plan.header_rewrites.iter().take(count) {
        headers.push(header_rewrite_result_from_c(header));
    }
    RewritePlan {
        phase,
        body_available: plan.body_available != 0,
        requires_body: plan.requires_body != 0,
        rewrite: rewrite_result_from_c(&plan.rewrite),
        header_rewrites: headers,
        header_rewrite_truncated: plan.header_rewrite_truncated != 0,
        script: script_result_from_c(&plan.script),
    }
}

fn cstr_from_buf(buf: &[c_char]) -> String {
    unsafe { CStr::from_ptr(buf.as_ptr()).to_string_lossy().into_owned() }
}

fn empty_header_rewrite_result() -> AnixopsHeaderRewriteResult {
    AnixopsHeaderRewriteResult {
        action: 0,
        phase: 0,
        rule_index: 0,
        matched_pattern: [0; ANIXOPS_PATTERN_CAP],
        header_name: [0; ANIXOPS_PATTERN_CAP],
        value: [0; ANIXOPS_VALUE_CAP],
        message: [0; ANIXOPS_MESSAGE_CAP],
    }
}

fn empty_rewrite_result() -> AnixopsRewriteResult {
    AnixopsRewriteResult {
        action: 0,
        status_code: 0,
        rule_index: 0,
        matched_pattern: [0; ANIXOPS_PATTERN_CAP],
        value: [0; ANIXOPS_VALUE_CAP],
        message: [0; ANIXOPS_MESSAGE_CAP],
    }
}

fn empty_rewrite_plan() -> AnixopsRewritePlan {
    AnixopsRewritePlan {
        phase: 0,
        body_available: 0,
        requires_body: 0,
        rewrite: empty_rewrite_result(),
        header_rewrite_count: 0,
        header_rewrite_truncated: 0,
        header_rewrites: [empty_header_rewrite_result(); ANIXOPS_PLAN_HEADER_CAP],
        script: empty_script_result(),
    }
}

fn empty_script_result() -> AnixopsScriptResult {
    AnixopsScriptResult {
        kind: 0,
        phase: 0,
        requires_body: 0,
        rule_index: 0,
        matched_pattern: [0; ANIXOPS_PATTERN_CAP],
        script_path: [0; ANIXOPS_VALUE_CAP],
        tag: [0; ANIXOPS_VALUE_CAP],
        argument: [0; ANIXOPS_ARGUMENT_CAP],
        message: [0; ANIXOPS_MESSAGE_CAP],
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    const FIXTURE_CONFIG: &str = r#"
[Argument]
Mode = select,rust

[Rewrite]
^http:\/\/old\.rust\.example\/(.*) https://api.rust.example/$1 302
^https:\/\/api\.rust\.example\/v1 response-header-add X-Rust rust-plan
^https:\/\/api\.rust\.example\/v1 response-body-replace-regex from to

[Script]
http-response ^https:\/\/api\.rust\.example\/v1 requires-body=1, script-path=https://scripts.example/rust-response.js, tag=rust.response, argument=[{Mode}]
"#;

    #[test]
    fn rust_binding_evaluates_policy() {
        assert_eq!(version(), "0.42.0");
        let mut engine = Engine::new().unwrap();
        engine.load_config(FIXTURE_CONFIG).unwrap();
        assert_eq!(engine.rewrite_rule_count(), 3);
        assert_eq!(engine.script_rule_count(), 1);

        let rewrite = engine
            .evaluate_rewrite("http://old.rust.example/v1", Phase::Request)
            .unwrap();
        assert_eq!(rewrite.action, RewriteAction::Redirect302);
        assert_eq!(rewrite.status_code, 302);
        assert_eq!(rewrite.value, "https://api.rust.example/v1");

        let (body, body_rewrite) = engine
            .apply_body("https://api.rust.example/v1", Phase::Response, "from=1")
            .unwrap();
        assert_eq!(body, "to=1");
        assert_eq!(body_rewrite.message, "body rewritten");

        let named_header = engine
            .evaluate_named_header("https://api.rust.example/v1", Phase::Response, 0, "x-rust", "")
            .unwrap();
        assert_eq!(named_header.action, RewriteAction::ResponseHeaderAdd);
        assert_eq!(named_header.header_name, "X-Rust");
        assert_eq!(named_header.value, "rust-plan");

        let missing_header = engine
            .evaluate_named_header("https://api.rust.example/v1", Phase::Response, 0, "x-missing", "")
            .unwrap();
        assert_eq!(missing_header.action, RewriteAction::None);

        let (plan, plan_body) = engine
            .build_plan("https://api.rust.example/v1", Phase::Response, "from=1")
            .unwrap();
        assert_eq!(plan.phase, Phase::Response);
        assert!(plan.body_available);
        assert!(plan.requires_body);
        assert_eq!(plan_body, "to=1");
        assert_eq!(plan.rewrite.action, RewriteAction::ResponseBodyReplaceRegex);
        assert_eq!(plan.rewrite.rule_index, 2);
        assert_eq!(plan.header_rewrites.len(), 1);
        assert_eq!(plan.header_rewrites[0].action, RewriteAction::ResponseHeaderAdd);
        assert_eq!(plan.header_rewrites[0].header_name, "X-Rust");
        assert_eq!(plan.header_rewrites[0].value, "rust-plan");
        assert!(!plan.header_rewrite_truncated);
        assert_eq!(plan.script.kind, ScriptKind::HttpResponse);
        assert_eq!(plan.script.script_path, "https://scripts.example/rust-response.js");

        let script = engine
            .evaluate_script("https://api.rust.example/v1", Phase::Response)
            .unwrap();
        assert_eq!(script.kind, ScriptKind::HttpResponse);
        assert!(script.requires_body);
        assert_eq!(script.script_path, "https://scripts.example/rust-response.js");
        assert_eq!(script.tag, "rust.response");
        assert_eq!(script.argument, "Mode=rust");
    }
}
