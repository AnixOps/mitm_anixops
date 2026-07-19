use std::ffi::{CStr, CString};
use std::fmt;
use std::os::raw::{c_char, c_int, c_uchar};

const ANIXOPS_OK: c_int = 0;
const ANIXOPS_VALUE_CAP: usize = 2048;
const ANIXOPS_ARGUMENT_CAP: usize = 4096;
const ANIXOPS_MESSAGE_CAP: usize = 256;
const ANIXOPS_PATTERN_CAP: usize = 256;
const ANIXOPS_PLAN_HEADER_CAP: usize = 16;
const ANIXOPS_HEADER_LIST_CAP: usize = 32;
const ANIXOPS_BODY_CHAIN_CAP: usize = 16;
pub const JQ_MAX_INPUT_BYTES_DEFAULT: usize = 1_048_576;
pub const JQ_MAX_OUTPUT_BYTES_DEFAULT: usize = 1_048_576;
pub const JQ_MAX_OUTPUT_VALUES_DEFAULT: usize = 64;
pub const JQ_MAX_EXECUTION_TIME_MS_DEFAULT: usize = 0;
pub const JQ_MAX_MEMORY_BYTES_DEFAULT: usize = 0;
pub const MAX_BODY_BYTES_DEFAULT: usize = 0;
pub const JQ_FILTER_CACHE_CAPACITY_DEFAULT: usize = 4;
pub const JQ_FILTER_CACHE_CAPACITY_MAX: usize = 16;
pub const POLICY_CAPABILITY_QUERY_ABI_VERSION: u32 = 1;
pub const POLICY_CAPABILITY_MITM_DECISION: u64 = 1 << 0;
pub const POLICY_CAPABILITY_URL_REWRITE: u64 = 1 << 1;
pub const POLICY_CAPABILITY_HEADER_MUTATION: u64 = 1 << 2;
pub const POLICY_CAPABILITY_BODY_MUTATION_BYTES: u64 = 1 << 3;
pub const POLICY_CAPABILITY_SCRIPT_DISPATCH_METADATA: u64 = 1 << 4;
pub const POLICY_CAPABILITY_RULE_DIAGNOSTICS: u64 = 1 << 5;
pub const POLICY_CAPABILITY_TASK_DESCRIPTOR_METADATA: u64 = 1 << 6;
pub const POLICY_CAPABILITY_ALL_V1: u64 = POLICY_CAPABILITY_MITM_DECISION
    | POLICY_CAPABILITY_URL_REWRITE
    | POLICY_CAPABILITY_HEADER_MUTATION
    | POLICY_CAPABILITY_BODY_MUTATION_BYTES
    | POLICY_CAPABILITY_SCRIPT_DISPATCH_METADATA
    | POLICY_CAPABILITY_RULE_DIAGNOSTICS
    | POLICY_CAPABILITY_TASK_DESCRIPTOR_METADATA;

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
struct AnixopsBodyRewriteChain {
    rewrite_count: usize,
    rewritten: c_int,
    truncated: c_int,
    rewrites: [AnixopsRewriteResult; ANIXOPS_BODY_CHAIN_CAP],
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
struct AnixopsHeaderField {
    name: [c_char; ANIXOPS_PATTERN_CAP],
    value: [c_char; ANIXOPS_VALUE_CAP],
}

#[repr(C)]
#[derive(Clone, Copy)]
struct AnixopsHeaderList {
    count: usize,
    truncated: c_int,
    fields: [AnixopsHeaderField; ANIXOPS_HEADER_LIST_CAP],
}

#[repr(C)]
#[derive(Clone, Copy)]
struct AnixopsScriptResult {
    kind: c_int,
    phase: c_int,
    requires_body: c_int,
    timeout_ms: usize,
    max_size: usize,
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
    fn anixops_policy_capability_query_abi_version() -> u32;
    fn anixops_policy_capability_flags() -> u64;
    fn anixops_engine_new() -> *mut AnixopsEngine;
    fn anixops_engine_free(engine: *mut AnixopsEngine);
    fn anixops_engine_load_config(engine: *mut AnixopsEngine, config_text: *const c_char) -> c_int;
    fn anixops_engine_rewrite_rule_count(engine: *const AnixopsEngine) -> usize;
    fn anixops_engine_script_rule_count(engine: *const AnixopsEngine) -> usize;
    fn anixops_engine_set_jq_max_input_bytes(
        engine: *mut AnixopsEngine,
        max_input_bytes: usize,
    ) -> c_int;
    fn anixops_engine_jq_max_input_bytes(engine: *const AnixopsEngine) -> usize;
    fn anixops_engine_set_jq_max_output_bytes(
        engine: *mut AnixopsEngine,
        max_output_bytes: usize,
    ) -> c_int;
    fn anixops_engine_jq_max_output_bytes(engine: *const AnixopsEngine) -> usize;
    fn anixops_engine_set_jq_max_output_values(
        engine: *mut AnixopsEngine,
        max_output_values: usize,
    ) -> c_int;
    fn anixops_engine_jq_max_output_values(engine: *const AnixopsEngine) -> usize;
    fn anixops_engine_set_jq_max_execution_time_ms(
        engine: *mut AnixopsEngine,
        max_execution_time_ms: usize,
    ) -> c_int;
    fn anixops_engine_jq_max_execution_time_ms(engine: *const AnixopsEngine) -> usize;
    fn anixops_engine_set_jq_max_memory_bytes(
        engine: *mut AnixopsEngine,
        max_memory_bytes: usize,
    ) -> c_int;
    fn anixops_engine_jq_max_memory_bytes(engine: *const AnixopsEngine) -> usize;
    fn anixops_engine_set_max_body_bytes(
        engine: *mut AnixopsEngine,
        max_body_bytes: usize,
    ) -> c_int;
    fn anixops_engine_max_body_bytes(engine: *const AnixopsEngine) -> usize;
    fn anixops_engine_jq_filter_cache_count(engine: *const AnixopsEngine) -> usize;
    fn anixops_engine_jq_filter_cache_hit_count(engine: *const AnixopsEngine) -> usize;
    fn anixops_engine_set_jq_filter_cache_capacity(
        engine: *mut AnixopsEngine,
        capacity: usize,
    ) -> c_int;
    fn anixops_engine_jq_filter_cache_capacity(engine: *const AnixopsEngine) -> usize;
    fn anixops_engine_clear_jq_filter_cache(engine: *mut AnixopsEngine) -> c_int;
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
    fn anixops_rewrite_apply_body_bytes(
        engine: *const AnixopsEngine,
        url: *const c_char,
        phase: c_int,
        body: *const c_uchar,
        body_len: usize,
        out_body: *mut c_uchar,
        out_body_cap: usize,
        out_body_len: *mut usize,
        out_result: *mut AnixopsRewriteResult,
    ) -> c_int;
    fn anixops_rewrite_apply_body_chain(
        engine: *const AnixopsEngine,
        url: *const c_char,
        phase: c_int,
        body: *const c_char,
        out_body: *mut c_char,
        out_body_cap: usize,
        out_chain: *mut AnixopsBodyRewriteChain,
    ) -> c_int;
    fn anixops_rewrite_apply_body_chain_bytes(
        engine: *const AnixopsEngine,
        url: *const c_char,
        phase: c_int,
        body: *const c_uchar,
        body_len: usize,
        out_body: *mut c_uchar,
        out_body_cap: usize,
        out_body_len: *mut usize,
        out_chain: *mut AnixopsBodyRewriteChain,
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
    fn anixops_rewrite_apply_headers(
        engine: *const AnixopsEngine,
        url: *const c_char,
        phase: c_int,
        headers: *const AnixopsHeaderList,
        out_headers: *mut AnixopsHeaderList,
        out_plan: *mut AnixopsRewritePlan,
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

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct PolicyCapabilitySet(u64);

impl PolicyCapabilitySet {
    pub fn supports(self, required: u64) -> bool {
        required == 0 || (self.0 & required) == required
    }
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
    RequestBodyReplaceRegex,
    ResponseBodyReplaceRegex,
    HeaderDel,
    HeaderAdd,
    HeaderReplace,
    HeaderReplaceRegex,
    ResponseHeaderDel,
    ResponseHeaderAdd,
    ResponseHeaderReplace,
    ResponseHeaderReplaceRegex,
    Other(i32),
}

impl From<c_int> for RewriteAction {
    fn from(value: c_int) -> Self {
        match value {
            0 => RewriteAction::None,
            1 => RewriteAction::Redirect302,
            11 => RewriteAction::RequestBodyReplaceRegex,
            12 => RewriteAction::ResponseBodyReplaceRegex,
            25 => RewriteAction::HeaderDel,
            14 => RewriteAction::HeaderAdd,
            13 => RewriteAction::HeaderReplace,
            15 => RewriteAction::HeaderReplaceRegex,
            16 => RewriteAction::ResponseHeaderDel,
            18 => RewriteAction::ResponseHeaderAdd,
            17 => RewriteAction::ResponseHeaderReplace,
            19 => RewriteAction::ResponseHeaderReplaceRegex,
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
pub struct BodyRewriteChain {
    pub rewrites: Vec<RewriteResult>,
    pub rewritten: bool,
    pub truncated: bool,
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
pub struct HeaderField {
    pub name: String,
    pub value: String,
}

#[derive(Debug, Eq, PartialEq)]
pub struct HeaderList {
    pub fields: Vec<HeaderField>,
    pub truncated: bool,
}

#[derive(Debug, Eq, PartialEq)]
pub struct ScriptResult {
    pub kind: ScriptKind,
    pub phase: Phase,
    pub requires_body: bool,
    pub timeout_ms: usize,
    pub max_size: usize,
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

    pub fn set_jq_max_input_bytes(&mut self, max_input_bytes: usize) -> Result<(), Error> {
        check_status(
            "set_jq_max_input_bytes",
            unsafe { anixops_engine_set_jq_max_input_bytes(self.ptr, max_input_bytes) },
        )
    }

    pub fn jq_max_input_bytes(&self) -> usize {
        unsafe { anixops_engine_jq_max_input_bytes(self.ptr) }
    }

    pub fn set_jq_max_output_bytes(&mut self, max_output_bytes: usize) -> Result<(), Error> {
        check_status(
            "set_jq_max_output_bytes",
            unsafe { anixops_engine_set_jq_max_output_bytes(self.ptr, max_output_bytes) },
        )
    }

    pub fn jq_max_output_bytes(&self) -> usize {
        unsafe { anixops_engine_jq_max_output_bytes(self.ptr) }
    }

    pub fn set_jq_max_output_values(&mut self, max_output_values: usize) -> Result<(), Error> {
        check_status(
            "set_jq_max_output_values",
            unsafe { anixops_engine_set_jq_max_output_values(self.ptr, max_output_values) },
        )
    }

    pub fn jq_max_output_values(&self) -> usize {
        unsafe { anixops_engine_jq_max_output_values(self.ptr) }
    }

    pub fn set_jq_max_execution_time_ms(&mut self, max_execution_time_ms: usize) -> Result<(), Error> {
        check_status(
            "set_jq_max_execution_time_ms",
            unsafe { anixops_engine_set_jq_max_execution_time_ms(self.ptr, max_execution_time_ms) },
        )
    }

    pub fn jq_max_execution_time_ms(&self) -> usize {
        unsafe { anixops_engine_jq_max_execution_time_ms(self.ptr) }
    }

    pub fn set_jq_max_memory_bytes(&mut self, max_memory_bytes: usize) -> Result<(), Error> {
        check_status(
            "set_jq_max_memory_bytes",
            unsafe { anixops_engine_set_jq_max_memory_bytes(self.ptr, max_memory_bytes) },
        )
    }

    pub fn jq_max_memory_bytes(&self) -> usize {
        unsafe { anixops_engine_jq_max_memory_bytes(self.ptr) }
    }

    pub fn set_max_body_bytes(&mut self, max_body_bytes: usize) -> Result<(), Error> {
        check_status(
            "set_max_body_bytes",
            unsafe { anixops_engine_set_max_body_bytes(self.ptr, max_body_bytes) },
        )
    }

    pub fn max_body_bytes(&self) -> usize {
        unsafe { anixops_engine_max_body_bytes(self.ptr) }
    }

    pub fn jq_filter_cache_count(&self) -> usize {
        unsafe { anixops_engine_jq_filter_cache_count(self.ptr) }
    }

    pub fn jq_filter_cache_hit_count(&self) -> usize {
        unsafe { anixops_engine_jq_filter_cache_hit_count(self.ptr) }
    }

    pub fn set_jq_filter_cache_capacity(&mut self, capacity: usize) -> Result<(), Error> {
        check_status(
            "set_jq_filter_cache_capacity",
            unsafe { anixops_engine_set_jq_filter_cache_capacity(self.ptr, capacity) },
        )
    }

    pub fn jq_filter_cache_capacity(&self) -> usize {
        unsafe { anixops_engine_jq_filter_cache_capacity(self.ptr) }
    }

    pub fn clear_jq_filter_cache(&mut self) -> Result<(), Error> {
        check_status(
            "clear_jq_filter_cache",
            unsafe { anixops_engine_clear_jq_filter_cache(self.ptr) },
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

    pub fn apply_body_bytes(
        &self,
        url: &str,
        phase: Phase,
        body: &[u8],
    ) -> Result<(Vec<u8>, RewriteResult), Error> {
        let url = CString::new(url).expect("url must not contain nul bytes");
        let mut result = empty_rewrite_result();
        let mut out = vec![0u8; body.len() + ANIXOPS_VALUE_CAP];
        let mut out_len = 0usize;
        let body_ptr = if body.is_empty() {
            std::ptr::null()
        } else {
            body.as_ptr()
        };
        check_status(
            "apply_body_bytes",
            unsafe {
                anixops_rewrite_apply_body_bytes(
                    self.ptr,
                    url.as_ptr(),
                    phase.as_c(),
                    body_ptr,
                    body.len(),
                    out.as_mut_ptr(),
                    out.len(),
                    &mut out_len,
                    &mut result,
                )
            },
        )?;
        if out_len > out.len() {
            return Err(Error {
                operation: "apply_body_bytes",
                status: -4,
                message: "body output length exceeds buffer".to_string(),
            });
        }
        out.truncate(out_len);
        Ok((out, rewrite_result_from_c(&result)))
    }

    pub fn apply_body_chain_bytes(
        &self,
        url: &str,
        phase: Phase,
        body: &[u8],
    ) -> Result<(Vec<u8>, BodyRewriteChain), Error> {
        let url = CString::new(url).expect("url must not contain nul bytes");
        let mut chain = empty_body_rewrite_chain();
        let mut out = vec![0u8; body.len() + ANIXOPS_VALUE_CAP];
        let mut out_len = 0usize;
        let body_ptr = if body.is_empty() {
            std::ptr::null()
        } else {
            body.as_ptr()
        };
        check_status(
            "apply_body_chain_bytes",
            unsafe {
                anixops_rewrite_apply_body_chain_bytes(
                    self.ptr,
                    url.as_ptr(),
                    phase.as_c(),
                    body_ptr,
                    body.len(),
                    out.as_mut_ptr(),
                    out.len(),
                    &mut out_len,
                    &mut chain,
                )
            },
        )?;
        if out_len > out.len() {
            return Err(Error {
                operation: "apply_body_chain_bytes",
                status: -4,
                message: "body output length exceeds buffer".to_string(),
            });
        }
        out.truncate(out_len);
        Ok((out, body_rewrite_chain_from_c(&chain)))
    }

    pub fn apply_body_chain(
        &self,
        url: &str,
        phase: Phase,
        body: &str,
    ) -> Result<(String, BodyRewriteChain), Error> {
        let url = CString::new(url).expect("url must not contain nul bytes");
        let body_c = CString::new(body).expect("body must not contain nul bytes");
        let mut chain = empty_body_rewrite_chain();
        let mut out = vec![0 as c_char; body.len() + ANIXOPS_VALUE_CAP];
        check_status(
            "apply_body_chain",
            unsafe {
                anixops_rewrite_apply_body_chain(
                    self.ptr,
                    url.as_ptr(),
                    phase.as_c(),
                    body_c.as_ptr(),
                    out.as_mut_ptr(),
                    out.len(),
                    &mut chain,
                )
            },
        )?;
        Ok((cstr_from_buf(&out), body_rewrite_chain_from_c(&chain)))
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

    pub fn apply_headers(
        &self,
        url: &str,
        phase: Phase,
        headers: &HeaderList,
    ) -> Result<(HeaderList, RewritePlan), Error> {
        let url = CString::new(url).expect("url must not contain nul bytes");
        let input = header_list_to_c(headers)?;
        let mut output = empty_header_list();
        let mut plan = empty_rewrite_plan();
        check_status(
            "apply_headers",
            unsafe {
                anixops_rewrite_apply_headers(
                    self.ptr,
                    url.as_ptr(),
                    phase.as_c(),
                    &input,
                    &mut output,
                    &mut plan,
                )
            },
        )?;
        Ok((header_list_from_c(&output), rewrite_plan_from_c(&plan)))
    }

    pub fn build_plan(
        &self,
        url: &str,
        phase: Phase,
        body: &str,
    ) -> Result<(RewritePlan, String), Error> {
        self.build_plan_inner(url, phase, body, None)
    }

    pub fn build_plan_with_current_header(
        &self,
        url: &str,
        phase: Phase,
        body: &str,
        current_header_value: &str,
    ) -> Result<(RewritePlan, String), Error> {
        self.build_plan_inner(url, phase, body, Some(current_header_value))
    }

    fn build_plan_inner(
        &self,
        url: &str,
        phase: Phase,
        body: &str,
        current_header_value: Option<&str>,
    ) -> Result<(RewritePlan, String), Error> {
        let url = CString::new(url).expect("url must not contain nul bytes");
        let body_c = CString::new(body).expect("body must not contain nul bytes");
        let current_header_value = current_header_value.map(|value| {
            CString::new(value).expect("current_header_value must not contain nul bytes")
        });
        let current_header_value_ptr = current_header_value
            .as_ref()
            .map_or(std::ptr::null(), |value| value.as_ptr());
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
                    current_header_value_ptr,
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

pub fn policy_capability_query_abi_version() -> u32 {
    unsafe { anixops_policy_capability_query_abi_version() }
}

pub fn policy_capabilities() -> PolicyCapabilitySet {
    PolicyCapabilitySet(unsafe { anixops_policy_capability_flags() })
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

fn body_rewrite_chain_from_c(chain: &AnixopsBodyRewriteChain) -> BodyRewriteChain {
    let count = chain.rewrite_count.min(ANIXOPS_BODY_CHAIN_CAP);
    let mut rewrites = Vec::with_capacity(count);
    for rewrite in chain.rewrites.iter().take(count) {
        rewrites.push(rewrite_result_from_c(rewrite));
    }
    BodyRewriteChain {
        rewrites,
        rewritten: chain.rewritten != 0,
        truncated: chain.truncated != 0,
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
        timeout_ms: result.timeout_ms,
        max_size: result.max_size,
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

fn header_list_to_c(headers: &HeaderList) -> Result<AnixopsHeaderList, Error> {
    if headers.fields.len() > ANIXOPS_HEADER_LIST_CAP {
        return Err(Error {
            operation: "header_list_to_c",
            status: -4,
            message: "header list exceeds capacity".to_string(),
        });
    }
    let mut out = empty_header_list();
    out.count = headers.fields.len();
    out.truncated = if headers.truncated { 1 } else { 0 };
    for (index, field) in headers.fields.iter().enumerate() {
        if write_str_to_buf(&mut out.fields[index].name, &field.name) {
            out.truncated = 1;
        }
        if write_str_to_buf(&mut out.fields[index].value, &field.value) {
            out.truncated = 1;
        }
    }
    Ok(out)
}

fn header_list_from_c(headers: &AnixopsHeaderList) -> HeaderList {
    let count = headers.count.min(ANIXOPS_HEADER_LIST_CAP);
    let mut fields = Vec::with_capacity(count);
    for field in headers.fields.iter().take(count) {
        fields.push(HeaderField {
            name: cstr_from_buf(&field.name),
            value: cstr_from_buf(&field.value),
        });
    }
    HeaderList {
        fields,
        truncated: headers.truncated != 0,
    }
}

fn write_str_to_buf(buf: &mut [c_char], value: &str) -> bool {
    for slot in buf.iter_mut() {
        *slot = 0;
    }
    if buf.is_empty() {
        return !value.is_empty();
    }
    let bytes = value.as_bytes();
    let copy_len = bytes.len().min(buf.len() - 1);
    for (index, byte) in bytes.iter().take(copy_len).enumerate() {
        buf[index] = *byte as c_char;
    }
    bytes.len() >= buf.len()
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

fn empty_header_field() -> AnixopsHeaderField {
    AnixopsHeaderField {
        name: [0; ANIXOPS_PATTERN_CAP],
        value: [0; ANIXOPS_VALUE_CAP],
    }
}

fn empty_header_list() -> AnixopsHeaderList {
    AnixopsHeaderList {
        count: 0,
        truncated: 0,
        fields: [empty_header_field(); ANIXOPS_HEADER_LIST_CAP],
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

fn empty_body_rewrite_chain() -> AnixopsBodyRewriteChain {
    AnixopsBodyRewriteChain {
        rewrite_count: 0,
        rewritten: 0,
        truncated: 0,
        rewrites: [empty_rewrite_result(); ANIXOPS_BODY_CHAIN_CAP],
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
        timeout_ms: 0,
        max_size: 0,
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

    const SHARED_PARITY_CONFIG: &str =
        include_str!("../../../../tests/fixtures/BindingParity.Common.conf");

    const FIXTURE_CONFIG: &str = r#"
[Argument]
Mode = select,rust

[Rewrite]
^http:\/\/old\.rust\.example\/(.*) https://api.rust.example/$1 302
^https:\/\/api\.rust\.example\/v1 response-header-add X-Rust rust-plan
^https:\/\/api\.rust\.example\/v1 response-body-replace-regex from to

[Script]
http-response ^https:\/\/api\.rust\.example\/v1 requires-body=1, timeout=4, max-size=2048, script-path=https://scripts.example/rust-response.js, tag=rust.response, argument=[{Mode}]
"#;

    #[test]
    fn rust_binding_evaluates_policy() {
        assert_eq!(version(), "0.45.10");
        assert_eq!(
            policy_capability_query_abi_version(),
            POLICY_CAPABILITY_QUERY_ABI_VERSION
        );
        let capabilities = policy_capabilities();
        assert!(capabilities.supports(POLICY_CAPABILITY_MITM_DECISION));
        assert!(capabilities.supports(POLICY_CAPABILITY_URL_REWRITE));
        assert!(capabilities.supports(POLICY_CAPABILITY_HEADER_MUTATION));
        assert!(capabilities.supports(POLICY_CAPABILITY_BODY_MUTATION_BYTES));
        assert!(capabilities.supports(POLICY_CAPABILITY_SCRIPT_DISPATCH_METADATA));
        assert!(capabilities.supports(POLICY_CAPABILITY_RULE_DIAGNOSTICS));
        assert!(capabilities.supports(POLICY_CAPABILITY_TASK_DESCRIPTOR_METADATA));
        assert!(capabilities.supports(POLICY_CAPABILITY_ALL_V1));
        assert!(capabilities.supports(0));
        assert!(!capabilities.supports(1_u64 << 63));
        let mut engine = Engine::new().unwrap();
        assert_eq!(engine.jq_max_input_bytes(), JQ_MAX_INPUT_BYTES_DEFAULT);
        assert_eq!(engine.jq_max_output_bytes(), JQ_MAX_OUTPUT_BYTES_DEFAULT);
        assert_eq!(engine.jq_max_output_values(), JQ_MAX_OUTPUT_VALUES_DEFAULT);
        assert_eq!(engine.jq_max_execution_time_ms(), JQ_MAX_EXECUTION_TIME_MS_DEFAULT);
        assert_eq!(engine.jq_max_memory_bytes(), JQ_MAX_MEMORY_BYTES_DEFAULT);
        assert_eq!(MAX_BODY_BYTES_DEFAULT, 0);
        assert_eq!(engine.max_body_bytes(), MAX_BODY_BYTES_DEFAULT);
        engine.set_max_body_bytes(4096).unwrap();
        assert_eq!(engine.max_body_bytes(), 4096);
        engine.set_max_body_bytes(0).unwrap();
        assert_eq!(engine.jq_filter_cache_count(), 0);
        assert_eq!(engine.jq_filter_cache_hit_count(), 0);
        assert_eq!(JQ_FILTER_CACHE_CAPACITY_DEFAULT, 4);
        assert_eq!(JQ_FILTER_CACHE_CAPACITY_MAX, 16);
        assert_eq!(engine.jq_filter_cache_capacity(), JQ_FILTER_CACHE_CAPACITY_DEFAULT);
        engine.set_jq_filter_cache_capacity(2).unwrap();
        assert_eq!(engine.jq_filter_cache_capacity(), 2);
        engine.clear_jq_filter_cache().unwrap();
        engine.set_jq_max_input_bytes(4096).unwrap();
        assert_eq!(engine.jq_max_input_bytes(), 4096);
        engine.set_jq_max_output_bytes(8192).unwrap();
        assert_eq!(engine.jq_max_output_bytes(), 8192);
        engine.set_jq_max_output_values(8).unwrap();
        assert_eq!(engine.jq_max_output_values(), 8);
        engine.set_jq_max_execution_time_ms(250).unwrap();
        assert_eq!(engine.jq_max_execution_time_ms(), 250);
        engine.set_jq_max_memory_bytes(32 * 1024 * 1024).unwrap();
        assert_eq!(engine.jq_max_memory_bytes(), 32 * 1024 * 1024);
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
        assert_eq!(plan.script.timeout_ms, 4000);
        assert_eq!(plan.script.max_size, 2048);

        let script = engine
            .evaluate_script("https://api.rust.example/v1", Phase::Response)
            .unwrap();
        assert_eq!(script.kind, ScriptKind::HttpResponse);
        assert!(script.requires_body);
        assert_eq!(script.script_path, "https://scripts.example/rust-response.js");
        assert_eq!(script.tag, "rust.response");
        assert_eq!(script.argument, "Mode=rust");
        assert_eq!(script.timeout_ms, 4000);
        assert_eq!(script.max_size, 2048);
    }

    #[test]
    fn rust_binding_loads_shared_parity_fixture() {
        let mut engine = Engine::new().unwrap();
        engine.load_config(SHARED_PARITY_CONFIG).unwrap();
        assert_eq!(engine.rewrite_rule_count(), 7);
        assert_eq!(engine.script_rule_count(), 2);

        let redirect = engine
            .evaluate_rewrite("http://old.binding.test/item", Phase::Request)
            .unwrap();
        assert_eq!(redirect.action, RewriteAction::Redirect302);
        assert_eq!(redirect.value, "https://api.binding.test/item");

        let (request_plan, request_body) = engine
            .build_plan("https://api.binding.test/request/item", Phase::Request, "token=42")
            .unwrap();
        assert_eq!(request_body, "token=42-bound");
        assert!(request_plan.body_available);
        assert!(request_plan.requires_body);
        assert_eq!(
            request_plan.rewrite.action,
            RewriteAction::RequestBodyReplaceRegex
        );
        assert_eq!(request_plan.header_rewrites.len(), 1);
        assert_eq!(request_plan.header_rewrites[0].action, RewriteAction::HeaderAdd);
        assert_eq!(request_plan.header_rewrites[0].header_name, "X-Binding");
        assert_eq!(request_plan.header_rewrites[0].value, "trace-item");
        assert_eq!(request_plan.script.kind, ScriptKind::HttpRequest);
        assert_eq!(request_plan.script.tag, "binding.request");
        assert_eq!(
            request_plan.script.script_path,
            "https://scripts.binding.test/request.js"
        );
        assert_eq!(request_plan.script.argument, "Mode=binding");
        assert_eq!(request_plan.script.timeout_ms, 333);
        assert_eq!(request_plan.script.max_size, 777);
        let request_header = engine
            .evaluate_named_header(
                "https://api.binding.test/request-current/item",
                Phase::Request,
                0,
                "x-binding-current",
                "old-item",
            )
            .unwrap();
        assert_eq!(request_header.action, RewriteAction::HeaderReplaceRegex);
        assert_eq!(request_header.header_name, "X-Binding-Current");
        assert_eq!(request_header.value, "req-item");
        let (request_header_plan, _) = engine
            .build_plan_with_current_header(
                "https://api.binding.test/request-current/item",
                Phase::Request,
                "",
                "old-item",
            )
            .unwrap();
        assert_eq!(request_header_plan.header_rewrites.len(), 1);
        assert_eq!(
            request_header_plan.header_rewrites[0].action,
            request_header.action
        );
        assert_eq!(
            request_header_plan.header_rewrites[0].header_name,
            request_header.header_name
        );
        assert_eq!(
            request_header_plan.header_rewrites[0].value,
            request_header.value
        );

        let (response_plan, response_body) = engine
            .build_plan(
                "https://api.binding.test/response/item",
                Phase::Response,
                "ad=1&ok=1",
            )
            .unwrap();
        assert_eq!(response_body, "clean=1&ok=1");
        assert_eq!(
            response_plan.rewrite.action,
            RewriteAction::ResponseBodyReplaceRegex
        );
        assert_eq!(response_plan.header_rewrites.len(), 1);
        assert_eq!(
            response_plan.header_rewrites[0].action,
            RewriteAction::ResponseHeaderAdd
        );
        assert_eq!(response_plan.header_rewrites[0].header_name, "X-Binding");
        assert_eq!(response_plan.header_rewrites[0].value, "resp-item");
        assert_eq!(response_plan.script.kind, ScriptKind::HttpResponse);
        assert_eq!(response_plan.script.tag, "binding.response");
        assert_eq!(
            response_plan.script.script_path,
            "https://scripts.binding.test/response.js"
        );
        assert_eq!(response_plan.script.argument, "Mode=binding");
        assert_eq!(response_plan.script.timeout_ms, 444);
        assert_eq!(response_plan.script.max_size, 888);
        let response_header = engine
            .evaluate_named_header(
                "https://api.binding.test/response-current/item",
                Phase::Response,
                0,
                "x-binding-current",
                "old-item",
            )
            .unwrap();
        assert_eq!(
            response_header.action,
            RewriteAction::ResponseHeaderReplaceRegex
        );
        assert_eq!(response_header.header_name, "X-Binding-Current");
        assert_eq!(response_header.value, "resp-item");
        let (response_header_plan, _) = engine
            .build_plan_with_current_header(
                "https://api.binding.test/response-current/item",
                Phase::Response,
                "",
                "old-item",
            )
            .unwrap();
        assert_eq!(response_header_plan.header_rewrites.len(), 1);
        assert_eq!(
            response_header_plan.header_rewrites[0].action,
            response_header.action
        );
        assert_eq!(
            response_header_plan.header_rewrites[0].header_name,
            response_header.header_name
        );
        assert_eq!(
            response_header_plan.header_rewrites[0].value,
            response_header.value
        );
    }

    #[test]
    fn rust_binding_applies_header_list() {
        let mut engine = Engine::new().unwrap();
        engine
            .load_config(
                r#"
[Rewrite]
^https:\/\/api\.rust\.example\/cookies response-header-add Set-Cookie "c=1; Path=/"
^https:\/\/api\.rust\.example\/cookies response-header-replace-regex Set-Cookie "a=1" "a=2"
^https:\/\/api\.rust\.example\/cookies response-header-del X-Remove
"#,
            )
            .unwrap();

        let input = HeaderList {
            fields: vec![
                HeaderField {
                    name: "Set-Cookie".to_string(),
                    value: "a=1; Path=/".to_string(),
                },
                HeaderField {
                    name: "set-cookie".to_string(),
                    value: "b=1; Path=/".to_string(),
                },
                HeaderField {
                    name: "X-Remove".to_string(),
                    value: "yes".to_string(),
                },
            ],
            truncated: false,
        };
        let (headers, plan) = engine
            .apply_headers("https://api.rust.example/cookies", Phase::Response, &input)
            .unwrap();

        assert!(!headers.truncated);
        assert_eq!(
            headers.fields,
            vec![
                HeaderField {
                    name: "Set-Cookie".to_string(),
                    value: "a=2; Path=/".to_string(),
                },
                HeaderField {
                    name: "set-cookie".to_string(),
                    value: "b=1; Path=/".to_string(),
                },
                HeaderField {
                    name: "Set-Cookie".to_string(),
                    value: "c=1; Path=/".to_string(),
                },
            ]
        );
        assert_eq!(plan.header_rewrites.len(), 3);
        assert_eq!(plan.header_rewrites[0].action, RewriteAction::ResponseHeaderAdd);
        assert_eq!(
            plan.header_rewrites[1].action,
            RewriteAction::ResponseHeaderReplaceRegex
        );
        assert_eq!(plan.header_rewrites[2].action, RewriteAction::ResponseHeaderDel);
    }

    #[test]
    fn rust_binding_applies_body_chain() {
        let mut engine = Engine::new().unwrap();
        engine
            .load_config(
                r#"
[Rewrite]
^https:\/\/api\.rust\.example\/chain request-body-replace-regex "from=([0-9]+)" "mid=$1"
^https:\/\/api\.rust\.example\/chain request-body-replace-regex "mid=([0-9]+)" "to=$1"
"#,
            )
            .unwrap();

        let (body, chain) = engine
            .apply_body_chain("https://api.rust.example/chain", Phase::Request, "from=42")
            .unwrap();
        assert_eq!(body, "to=42");
        assert!(chain.rewritten);
        assert!(!chain.truncated);
        assert_eq!(chain.rewrites.len(), 2);
        assert_eq!(chain.rewrites[0].action, RewriteAction::RequestBodyReplaceRegex);
        assert_eq!(chain.rewrites[1].action, RewriteAction::RequestBodyReplaceRegex);
    }

    #[test]
    fn rust_binding_applies_body_bytes() {
        let mut engine = Engine::new().unwrap();
        engine
            .load_config(
                r#"
[Rewrite]
^https:\/\/api\.rust\.example\/bytes request-body-replace-regex "from=([0-9]+)" "to=$1"
"#,
            )
            .unwrap();

        let (body, rewrite) = engine
            .apply_body_bytes(
                "https://api.rust.example/bytes",
                Phase::Request,
                b"from=1\0x",
            )
            .unwrap();
        assert_eq!(body, b"from=1\0x");
        assert_eq!(rewrite.message, "binary body passthrough");
        let (chain_body, chain) = engine
            .apply_body_chain_bytes(
                "https://api.rust.example/bytes",
                Phase::Request,
                b"from=1\0x",
            )
            .unwrap();
        assert_eq!(chain_body, b"from=1\0x");
        assert!(!chain.rewritten);
        assert!(!chain.truncated);
        assert_eq!(chain.rewrites.len(), 1);
        assert_eq!(chain.rewrites[0].message, "binary body passthrough");
    }
}
