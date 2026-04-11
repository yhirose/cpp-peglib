pub mod ffi;

use ffi::*;
use std::ffi::{CStr, CString};
use std::ptr;

fn c(s: &str) -> CString { CString::new(s).unwrap() }

pub use ffi::LLVMIntPredicate;

// --- Context ---

pub struct Context { raw: LLVMContextRef }

impl Context {
    pub fn new() -> Self { Self { raw: unsafe { LLVMContextCreate() } } }
    pub fn raw(&self) -> LLVMContextRef { self.raw }

    pub fn i1_type(&self) -> LLVMTypeRef { unsafe { LLVMInt1TypeInContext(self.raw) } }
    pub fn i32_type(&self) -> LLVMTypeRef { unsafe { LLVMInt32TypeInContext(self.raw) } }
    pub fn i64_type(&self) -> LLVMTypeRef { unsafe { LLVMInt64TypeInContext(self.raw) } }
    pub fn void_type(&self) -> LLVMTypeRef { unsafe { LLVMVoidTypeInContext(self.raw) } }
    pub fn ptr_type(&self) -> LLVMTypeRef { unsafe { LLVMPointerTypeInContext(self.raw, 0) } }

    pub fn function_type(&self, ret: LLVMTypeRef, params: &[LLVMTypeRef], var_arg: bool) -> LLVMTypeRef {
        unsafe { LLVMFunctionType(ret, params.as_ptr() as *mut _, params.len() as u32, var_arg as i32) }
    }

    pub fn struct_type(&self, elems: &[LLVMTypeRef]) -> LLVMTypeRef {
        unsafe { LLVMStructTypeInContext(self.raw, elems.as_ptr() as *mut _, elems.len() as u32, 0) }
    }

    pub fn append_bb(&self, func: LLVMValueRef, name: &str) -> LLVMBasicBlockRef {
        let n = c(name);
        unsafe { LLVMAppendBasicBlockInContext(self.raw, func, n.as_ptr()) }
    }

    pub fn create_bb(&self, name: &str) -> LLVMBasicBlockRef {
        let n = c(name);
        unsafe { LLVMCreateBasicBlockInContext(self.raw, n.as_ptr()) }
    }
}

impl Drop for Context {
    fn drop(&mut self) { unsafe { LLVMContextDispose(self.raw); } }
}

// --- Module ---

pub struct Module { raw: LLVMModuleRef, owned: bool }

impl Module {
    pub fn new(name: &str, ctx: &Context) -> Self {
        let n = c(name);
        Self { raw: unsafe { LLVMModuleCreateWithNameInContext(n.as_ptr(), ctx.raw()) }, owned: true }
    }

    pub fn raw(&self) -> LLVMModuleRef { self.raw }

    pub fn take(mut self) -> LLVMModuleRef {
        self.owned = false;
        self.raw
    }

    pub fn add_function(&self, name: &str, ty: LLVMTypeRef) -> LLVMValueRef {
        let n = c(name);
        unsafe { LLVMAddFunction(self.raw, n.as_ptr(), ty) }
    }

    pub fn get_function(&self, name: &str) -> Option<LLVMValueRef> {
        let n = c(name);
        let f = unsafe { LLVMGetNamedFunction(self.raw, n.as_ptr()) };
        if f.is_null() { None } else { Some(f) }
    }

    pub fn get_or_insert_function(&self, name: &str, ty: LLVMTypeRef) -> LLVMValueRef {
        let n = c(name);
        unsafe { LLVMGetOrInsertFunction(self.raw, n.as_ptr(), name.len(), ty) }
    }

    pub fn add_global(&self, ty: LLVMTypeRef, name: &str) -> LLVMValueRef {
        let n = c(name);
        unsafe { LLVMAddGlobal(self.raw, ty, n.as_ptr()) }
    }

    pub fn print_to_string(&self) -> String {
        unsafe {
            let ptr = LLVMPrintModuleToString(self.raw);
            let s = CStr::from_ptr(ptr).to_string_lossy().into_owned();
            LLVMDisposeMessage(ptr);
            s
        }
    }
}

impl Drop for Module {
    fn drop(&mut self) { if self.owned { unsafe { LLVMDisposeModule(self.raw); } } }
}

// --- Builder ---

pub struct Builder { raw: LLVMBuilderRef }

impl Builder {
    pub fn new(ctx: &Context) -> Self {
        Self { raw: unsafe { LLVMCreateBuilderInContext(ctx.raw()) } }
    }

    pub fn position_at_end(&self, bb: LLVMBasicBlockRef) {
        unsafe { LLVMPositionBuilderAtEnd(self.raw, bb); }
    }

    pub fn get_insert_block(&self) -> LLVMBasicBlockRef {
        unsafe { LLVMGetInsertBlock(self.raw) }
    }

    pub fn current_function(&self) -> LLVMValueRef {
        unsafe { LLVMGetBasicBlockParent(self.get_insert_block()) }
    }

    // Arithmetic
    pub fn add(&self, lhs: LLVMValueRef, rhs: LLVMValueRef, name: &str) -> LLVMValueRef {
        let n = c(name); unsafe { LLVMBuildAdd(self.raw, lhs, rhs, n.as_ptr()) }
    }
    pub fn sub(&self, lhs: LLVMValueRef, rhs: LLVMValueRef, name: &str) -> LLVMValueRef {
        let n = c(name); unsafe { LLVMBuildSub(self.raw, lhs, rhs, n.as_ptr()) }
    }
    pub fn mul(&self, lhs: LLVMValueRef, rhs: LLVMValueRef, name: &str) -> LLVMValueRef {
        let n = c(name); unsafe { LLVMBuildMul(self.raw, lhs, rhs, n.as_ptr()) }
    }
    pub fn sdiv(&self, lhs: LLVMValueRef, rhs: LLVMValueRef, name: &str) -> LLVMValueRef {
        let n = c(name); unsafe { LLVMBuildSDiv(self.raw, lhs, rhs, n.as_ptr()) }
    }
    pub fn neg(&self, v: LLVMValueRef, name: &str) -> LLVMValueRef {
        let n = c(name); unsafe { LLVMBuildNeg(self.raw, v, n.as_ptr()) }
    }
    pub fn icmp(&self, op: LLVMIntPredicate, lhs: LLVMValueRef, rhs: LLVMValueRef, name: &str) -> LLVMValueRef {
        let n = c(name); unsafe { LLVMBuildICmp(self.raw, op, lhs, rhs, n.as_ptr()) }
    }

    // Control flow
    pub fn br(&self, dest: LLVMBasicBlockRef) { unsafe { LLVMBuildBr(self.raw, dest); } }
    pub fn cond_br(&self, cond: LLVMValueRef, then_bb: LLVMBasicBlockRef, else_bb: LLVMBasicBlockRef) {
        unsafe { LLVMBuildCondBr(self.raw, cond, then_bb, else_bb); }
    }
    pub fn ret_void(&self) { unsafe { LLVMBuildRetVoid(self.raw); } }
    pub fn unreachable(&self) { unsafe { LLVMBuildUnreachable(self.raw); } }

    // Calls
    pub fn call(&self, fn_ty: LLVMTypeRef, func: LLVMValueRef, args: &[LLVMValueRef], name: &str) -> LLVMValueRef {
        let n = c(name);
        unsafe { LLVMBuildCall2(self.raw, fn_ty, func, args.as_ptr() as *mut _, args.len() as u32, n.as_ptr()) }
    }

    pub fn invoke(&self, fn_ty: LLVMTypeRef, func: LLVMValueRef, args: &[LLVMValueRef], then_bb: LLVMBasicBlockRef, catch_bb: LLVMBasicBlockRef, name: &str) -> LLVMValueRef {
        let n = c(name);
        unsafe { LLVMBuildInvoke2(self.raw, fn_ty, func, args.as_ptr() as *mut _, args.len() as u32, then_bb, catch_bb, n.as_ptr()) }
    }

    // Memory
    pub fn alloca(&self, ty: LLVMTypeRef, name: &str) -> LLVMValueRef {
        let n = c(name); unsafe { LLVMBuildAlloca(self.raw, ty, n.as_ptr()) }
    }
    pub fn load(&self, ty: LLVMTypeRef, ptr: LLVMValueRef, name: &str) -> LLVMValueRef {
        let n = c(name); unsafe { LLVMBuildLoad2(self.raw, ty, ptr, n.as_ptr()) }
    }
    pub fn store(&self, val: LLVMValueRef, ptr: LLVMValueRef) {
        unsafe { LLVMBuildStore(self.raw, val, ptr); }
    }

    // Strings
    pub fn global_string_ptr(&self, s: &str, name: &str) -> LLVMValueRef {
        let sv = c(s); let n = c(name);
        unsafe { LLVMBuildGlobalStringPtr(self.raw, sv.as_ptr(), n.as_ptr()) }
    }

    // Exception handling
    pub fn landing_pad(&self, ty: LLVMTypeRef, num_clauses: u32, name: &str) -> LLVMValueRef {
        let n = c(name);
        unsafe { LLVMBuildLandingPad(self.raw, ty, ptr::null_mut(), num_clauses, n.as_ptr()) }
    }
    pub fn extract_value(&self, agg: LLVMValueRef, idx: u32, name: &str) -> LLVMValueRef {
        let n = c(name); unsafe { LLVMBuildExtractValue(self.raw, agg, idx, n.as_ptr()) }
    }

    // Constants
    pub fn const_i32(&self, ctx: &Context, val: i64) -> LLVMValueRef {
        unsafe { LLVMConstInt(ctx.i32_type(), val as u64, 0) }
    }
    pub fn const_i64(&self, ctx: &Context, val: i64) -> LLVMValueRef {
        unsafe { LLVMConstInt(ctx.i64_type(), val as u64, 0) }
    }
}

impl Drop for Builder {
    fn drop(&mut self) { unsafe { LLVMDisposeBuilder(self.raw); } }
}

// --- Value helpers ---

pub fn set_value_name(val: LLVMValueRef, name: &str) {
    unsafe { LLVMSetValueName2(val, name.as_ptr() as *const i8, name.len()); }
}

pub fn get_param(func: LLVMValueRef, idx: u32) -> LLVMValueRef {
    unsafe { LLVMGetParam(func, idx) }
}

pub fn count_params(func: LLVMValueRef) -> u32 {
    unsafe { LLVMCountParams(func) }
}

pub fn set_personality_fn(func: LLVMValueRef, personality: LLVMValueRef) {
    unsafe { LLVMSetPersonalityFn(func, personality); }
}

pub fn set_linkage(val: LLVMValueRef, linkage: LLVMLinkage) {
    unsafe { LLVMSetLinkage(val, linkage); }
}

pub fn set_initializer(global: LLVMValueRef, val: LLVMValueRef) {
    unsafe { LLVMSetInitializer(global, val); }
}

pub fn set_global_constant(global: LLVMValueRef, is_constant: bool) {
    unsafe { LLVMSetGlobalConstant(global, is_constant as i32); }
}

pub fn add_clause(landing_pad: LLVMValueRef, clause: LLVMValueRef) {
    unsafe { LLVMAddClause(landing_pad, clause); }
}

pub fn const_null_ptr(ctx: &Context) -> LLVMValueRef {
    unsafe { LLVMConstPointerNull(ctx.ptr_type()) }
}

pub fn verify_function(func: LLVMValueRef) -> bool {
    unsafe { LLVMVerifyFunction(func, LLVMVerifierFailureAction::LLVMPrintMessageAction) == 0 }
}

pub fn append_bb_after_insert(_ctx: &Context, _builder: &Builder, func: LLVMValueRef, bb: LLVMBasicBlockRef) {
    let last = unsafe { LLVMGetLastBasicBlock(func) };
    if !last.is_null() {
        unsafe { LLVMMoveBasicBlockAfter(bb, last); }
    }
}

// --- ORC JIT ---

pub struct LLJIT { raw: LLVMOrcLLJITRef }

impl LLJIT {
    pub fn new() -> Result<Self, String> {
        initialize_native_target();
        let mut jit: LLVMOrcLLJITRef = ptr::null_mut();
        let builder = unsafe { LLVMOrcCreateLLJITBuilder() };
        let err = unsafe { LLVMOrcCreateLLJIT(&mut jit, builder) };
        check_error(err)?;
        Ok(Self { raw: jit })
    }

    pub fn add_module(&self, ctx: Context, module: Module) -> Result<(), String> {
        let ts_ctx = unsafe { LLVMOrcCreateNewThreadSafeContext() };
        // Module and context ownership transfers to ThreadSafeModule
        let ts_mod = unsafe { LLVMOrcCreateNewThreadSafeModule(module.take(), ts_ctx) };
        std::mem::forget(ctx); // ORC takes ownership

        let jd = unsafe { LLVMOrcLLJITGetMainJITDylib(self.raw) };

        // Add dynamic library search for libc functions (printf, etc.)
        let prefix = unsafe { LLVMOrcLLJITGetGlobalPrefix(self.raw) };
        let mut gen: LLVMOrcDefinitionGeneratorRef = ptr::null_mut();
        let err = unsafe { LLVMOrcCreateDynamicLibrarySearchGeneratorForProcess(&mut gen, prefix, None, ptr::null_mut()) };
        check_error(err)?;
        unsafe { LLVMOrcJITDylibAddGenerator(jd, gen); }

        let err = unsafe { LLVMOrcLLJITAddLLVMIRModule(self.raw, jd, ts_mod) };
        check_error(err)?;
        Ok(())
    }

    pub fn lookup(&self, name: &str) -> Result<u64, String> {
        let n = c(name);
        let mut addr: LLVMOrcExecutorAddress = 0;
        let err = unsafe { LLVMOrcLLJITLookup(self.raw, &mut addr, n.as_ptr()) };
        check_error(err)?;
        Ok(addr)
    }
}

impl Drop for LLJIT {
    fn drop(&mut self) {
        if !self.raw.is_null() {
            unsafe { LLVMOrcDisposeLLJIT(self.raw); }
        }
    }
}

pub fn initialize_native_target() {
    ffi::initialize_native_target();
}

fn check_error(err: LLVMErrorRef) -> Result<(), String> {
    if err.is_null() {
        Ok(())
    } else {
        let msg = unsafe {
            let ptr = LLVMGetErrorMessage(err);
            let s = CStr::from_ptr(ptr).to_string_lossy().into_owned();
            LLVMDisposeMessage(ptr);
            s
        };
        Err(msg)
    }
}
