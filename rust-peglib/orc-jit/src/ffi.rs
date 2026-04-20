#![allow(non_camel_case_types, dead_code)]

use std::os::raw::{c_char, c_uint, c_int, c_ulonglong};

// Opaque types
pub enum LLVMContext {}
pub enum LLVMModule {}
pub enum LLVMBuilder {}
pub enum LLVMType {}
pub enum LLVMValue {}
pub enum LLVMBasicBlock {}
pub enum LLVMOpaqueError {}
pub enum LLVMOrcOpaqueThreadSafeContext {}
pub enum LLVMOrcOpaqueThreadSafeModule {}
pub enum LLVMOrcOpaqueLLJITBuilder {}
pub enum LLVMOrcOpaqueLLJIT {}
pub enum LLVMOrcOpaqueExecutionSession {}
pub enum LLVMOrcOpaqueJITDylib {}
pub enum LLVMOrcOpaqueDefinitionGenerator {}
pub enum LLVMOrcOpaqueSymbolStringPoolEntry {}
pub enum LLVMOpaqueMemoryBuffer {}

pub type LLVMContextRef = *mut LLVMContext;
pub type LLVMModuleRef = *mut LLVMModule;
pub type LLVMBuilderRef = *mut LLVMBuilder;
pub type LLVMTypeRef = *mut LLVMType;
pub type LLVMValueRef = *mut LLVMValue;
pub type LLVMBasicBlockRef = *mut LLVMBasicBlock;
pub type LLVMBool = c_int;
pub type LLVMErrorRef = *mut LLVMOpaqueError;
pub type LLVMOrcThreadSafeContextRef = *mut LLVMOrcOpaqueThreadSafeContext;
pub type LLVMOrcThreadSafeModuleRef = *mut LLVMOrcOpaqueThreadSafeModule;
pub type LLVMOrcLLJITBuilderRef = *mut LLVMOrcOpaqueLLJITBuilder;
pub type LLVMOrcLLJITRef = *mut LLVMOrcOpaqueLLJIT;
pub type LLVMOrcExecutionSessionRef = *mut LLVMOrcOpaqueExecutionSession;
pub type LLVMOrcJITDylibRef = *mut LLVMOrcOpaqueJITDylib;
pub type LLVMOrcDefinitionGeneratorRef = *mut LLVMOrcOpaqueDefinitionGenerator;
pub type LLVMOrcExecutorAddress = u64;
pub type LLVMOrcSymbolPredicate = Option<extern "C" fn(*mut std::ffi::c_void, LLVMOrcSymbolStringPoolEntryRef) -> c_int>;
pub type LLVMOrcSymbolStringPoolEntryRef = *mut LLVMOrcOpaqueSymbolStringPoolEntry;

// Enums
#[repr(C)]
pub enum LLVMIntPredicate {
    LLVMIntEQ = 32,
    LLVMIntNE = 33,
    LLVMIntUGT = 34,
    LLVMIntUGE = 35,
    LLVMIntULT = 36,
    LLVMIntULE = 37,
    LLVMIntSGT = 38,
    LLVMIntSGE = 39,
    LLVMIntSLT = 40,
    LLVMIntSLE = 41,
}

#[repr(C)]
pub enum LLVMLinkage {
    LLVMExternalLinkage = 0,
    LLVMAvailableExternallyLinkage = 1,
    LLVMLinkOnceAnyLinkage = 2,
    LLVMLinkOnceODRLinkage = 3,
    LLVMWeakAnyLinkage = 5,
    LLVMWeakODRLinkage = 6,
    LLVMAppendingLinkage = 7,
    LLVMInternalLinkage = 8,
    LLVMPrivateLinkage = 9,
    LLVMExternalWeakLinkage = 12,
    LLVMCommonLinkage = 14,
}

#[repr(C)]
pub enum LLVMVerifierFailureAction {
    LLVMAbortProcessAction = 0,
    LLVMPrintMessageAction = 1,
    LLVMReturnStatusAction = 2,
}

unsafe extern "C" {
    // Context
    pub fn LLVMContextCreate() -> LLVMContextRef;
    pub fn LLVMContextDispose(C: LLVMContextRef);

    // Module
    pub fn LLVMModuleCreateWithNameInContext(ModuleID: *const c_char, C: LLVMContextRef) -> LLVMModuleRef;
    pub fn LLVMDisposeModule(M: LLVMModuleRef);
    pub fn LLVMPrintModuleToString(M: LLVMModuleRef) -> *mut c_char;
    pub fn LLVMDisposeMessage(Message: *mut c_char);

    // Builder
    pub fn LLVMCreateBuilderInContext(C: LLVMContextRef) -> LLVMBuilderRef;
    pub fn LLVMDisposeBuilder(Builder: LLVMBuilderRef);
    pub fn LLVMPositionBuilderAtEnd(Builder: LLVMBuilderRef, Block: LLVMBasicBlockRef);
    pub fn LLVMGetInsertBlock(Builder: LLVMBuilderRef) -> LLVMBasicBlockRef;

    // Types
    pub fn LLVMInt1TypeInContext(C: LLVMContextRef) -> LLVMTypeRef;
    pub fn LLVMInt32TypeInContext(C: LLVMContextRef) -> LLVMTypeRef;
    pub fn LLVMInt64TypeInContext(C: LLVMContextRef) -> LLVMTypeRef;
    pub fn LLVMVoidTypeInContext(C: LLVMContextRef) -> LLVMTypeRef;
    pub fn LLVMPointerTypeInContext(C: LLVMContextRef, AddressSpace: c_uint) -> LLVMTypeRef;
    pub fn LLVMFunctionType(ReturnType: LLVMTypeRef, ParamTypes: *mut LLVMTypeRef, ParamCount: c_uint, IsVarArg: LLVMBool) -> LLVMTypeRef;
    pub fn LLVMStructTypeInContext(C: LLVMContextRef, ElementTypes: *mut LLVMTypeRef, ElementCount: c_uint, Packed: LLVMBool) -> LLVMTypeRef;

    // Functions
    pub fn LLVMAddFunction(M: LLVMModuleRef, Name: *const c_char, FunctionTy: LLVMTypeRef) -> LLVMValueRef;
    pub fn LLVMGetNamedFunction(M: LLVMModuleRef, Name: *const c_char) -> LLVMValueRef;
    pub fn LLVMGetParam(Fn: LLVMValueRef, Index: c_uint) -> LLVMValueRef;
    pub fn LLVMSetValueName2(Val: LLVMValueRef, Name: *const c_char, NameLen: usize);
    pub fn LLVMCountParams(Fn: LLVMValueRef) -> c_uint;
    pub fn LLVMSetPersonalityFn(Fn: LLVMValueRef, PersonalityFn: LLVMValueRef);
    pub fn LLVMSetLinkage(Global: LLVMValueRef, Linkage: LLVMLinkage);
    pub fn LLVMGetOrInsertFunction(M: LLVMModuleRef, Name: *const c_char, NameLen: usize, FunctionTy: LLVMTypeRef) -> LLVMValueRef;

    // Basic blocks
    pub fn LLVMAppendBasicBlockInContext(C: LLVMContextRef, Fn: LLVMValueRef, Name: *const c_char) -> LLVMBasicBlockRef;
    pub fn LLVMCreateBasicBlockInContext(C: LLVMContextRef, Name: *const c_char) -> LLVMBasicBlockRef;
    pub fn LLVMGetLastBasicBlock(Fn: LLVMValueRef) -> LLVMBasicBlockRef;
    pub fn LLVMMoveBasicBlockAfter(BB: LLVMBasicBlockRef, MovePos: LLVMBasicBlockRef);
    pub fn LLVMGetBasicBlockParent(BB: LLVMBasicBlockRef) -> LLVMValueRef;
    pub fn LLVMAppendExistingBasicBlock(Fn: LLVMValueRef, BB: LLVMBasicBlockRef);

    // Arithmetic instructions
    pub fn LLVMBuildAdd(B: LLVMBuilderRef, LHS: LLVMValueRef, RHS: LLVMValueRef, Name: *const c_char) -> LLVMValueRef;
    pub fn LLVMBuildSub(B: LLVMBuilderRef, LHS: LLVMValueRef, RHS: LLVMValueRef, Name: *const c_char) -> LLVMValueRef;
    pub fn LLVMBuildMul(B: LLVMBuilderRef, LHS: LLVMValueRef, RHS: LLVMValueRef, Name: *const c_char) -> LLVMValueRef;
    pub fn LLVMBuildSDiv(B: LLVMBuilderRef, LHS: LLVMValueRef, RHS: LLVMValueRef, Name: *const c_char) -> LLVMValueRef;
    pub fn LLVMBuildNeg(B: LLVMBuilderRef, V: LLVMValueRef, Name: *const c_char) -> LLVMValueRef;
    pub fn LLVMBuildICmp(B: LLVMBuilderRef, Op: LLVMIntPredicate, LHS: LLVMValueRef, RHS: LLVMValueRef, Name: *const c_char) -> LLVMValueRef;

    // Control flow
    pub fn LLVMBuildBr(B: LLVMBuilderRef, Dest: LLVMBasicBlockRef) -> LLVMValueRef;
    pub fn LLVMBuildCondBr(B: LLVMBuilderRef, If: LLVMValueRef, Then: LLVMBasicBlockRef, Else: LLVMBasicBlockRef) -> LLVMValueRef;
    pub fn LLVMBuildRet(B: LLVMBuilderRef, V: LLVMValueRef) -> LLVMValueRef;
    pub fn LLVMBuildRetVoid(B: LLVMBuilderRef) -> LLVMValueRef;
    pub fn LLVMBuildUnreachable(B: LLVMBuilderRef) -> LLVMValueRef;

    // Calls / Invoke
    pub fn LLVMBuildCall2(B: LLVMBuilderRef, Ty: LLVMTypeRef, Fn: LLVMValueRef, Args: *mut LLVMValueRef, NumArgs: c_uint, Name: *const c_char) -> LLVMValueRef;
    pub fn LLVMBuildInvoke2(B: LLVMBuilderRef, Ty: LLVMTypeRef, Fn: LLVMValueRef, Args: *mut LLVMValueRef, NumArgs: c_uint, Then: LLVMBasicBlockRef, Catch: LLVMBasicBlockRef, Name: *const c_char) -> LLVMValueRef;

    // Memory
    pub fn LLVMBuildAlloca(B: LLVMBuilderRef, Ty: LLVMTypeRef, Name: *const c_char) -> LLVMValueRef;
    pub fn LLVMBuildLoad2(B: LLVMBuilderRef, Ty: LLVMTypeRef, PointerVal: LLVMValueRef, Name: *const c_char) -> LLVMValueRef;
    pub fn LLVMBuildStore(B: LLVMBuilderRef, Val: LLVMValueRef, Ptr: LLVMValueRef) -> LLVMValueRef;

    // Strings
    pub fn LLVMBuildGlobalStringPtr(B: LLVMBuilderRef, Str: *const c_char, Name: *const c_char) -> LLVMValueRef;

    // Exception handling
    pub fn LLVMBuildLandingPad(B: LLVMBuilderRef, Ty: LLVMTypeRef, PersFn: LLVMValueRef, NumClauses: c_uint, Name: *const c_char) -> LLVMValueRef;
    pub fn LLVMAddClause(LandingPad: LLVMValueRef, ClauseVal: LLVMValueRef);
    pub fn LLVMBuildExtractValue(B: LLVMBuilderRef, AggVal: LLVMValueRef, Index: c_uint, Name: *const c_char) -> LLVMValueRef;

    // Constants
    pub fn LLVMConstInt(IntTy: LLVMTypeRef, N: c_ulonglong, SignExtend: LLVMBool) -> LLVMValueRef;
    pub fn LLVMConstPointerNull(Ty: LLVMTypeRef) -> LLVMValueRef;

    // Globals
    pub fn LLVMAddGlobal(M: LLVMModuleRef, Ty: LLVMTypeRef, Name: *const c_char) -> LLVMValueRef;
    pub fn LLVMSetInitializer(GlobalVar: LLVMValueRef, ConstantVal: LLVMValueRef);
    pub fn LLVMSetGlobalConstant(GlobalVar: LLVMValueRef, IsConstant: LLVMBool);

    // Verification
    pub fn LLVMVerifyFunction(Fn: LLVMValueRef, Action: LLVMVerifierFailureAction) -> LLVMBool;

    // Target initialization
    pub fn LLVMInitializeX86TargetInfo();
    pub fn LLVMInitializeX86Target();
    pub fn LLVMInitializeX86TargetMC();
    pub fn LLVMInitializeX86AsmPrinter();
    pub fn LLVMInitializeAArch64TargetInfo();
    pub fn LLVMInitializeAArch64Target();
    pub fn LLVMInitializeAArch64TargetMC();
    pub fn LLVMInitializeAArch64AsmPrinter();

    // ORC JIT
    pub fn LLVMOrcCreateNewThreadSafeContext() -> LLVMOrcThreadSafeContextRef;
    pub fn LLVMOrcDisposeThreadSafeContext(TSCtx: LLVMOrcThreadSafeContextRef);
    pub fn LLVMOrcCreateNewThreadSafeModule(M: LLVMModuleRef, TSCtx: LLVMOrcThreadSafeContextRef) -> LLVMOrcThreadSafeModuleRef;
    pub fn LLVMOrcDisposeThreadSafeModule(TSM: LLVMOrcThreadSafeModuleRef);
    pub fn LLVMOrcCreateLLJITBuilder() -> LLVMOrcLLJITBuilderRef;
    pub fn LLVMOrcDisposeLLJITBuilder(Builder: LLVMOrcLLJITBuilderRef);
    pub fn LLVMOrcCreateLLJIT(Result: *mut LLVMOrcLLJITRef, Builder: LLVMOrcLLJITBuilderRef) -> LLVMErrorRef;
    pub fn LLVMOrcDisposeLLJIT(J: LLVMOrcLLJITRef) -> LLVMErrorRef;
    pub fn LLVMOrcLLJITGetMainJITDylib(J: LLVMOrcLLJITRef) -> LLVMOrcJITDylibRef;
    pub fn LLVMOrcLLJITGetGlobalPrefix(J: LLVMOrcLLJITRef) -> c_char;
    pub fn LLVMOrcLLJITAddLLVMIRModule(J: LLVMOrcLLJITRef, JD: LLVMOrcJITDylibRef, TSM: LLVMOrcThreadSafeModuleRef) -> LLVMErrorRef;
    pub fn LLVMOrcLLJITLookup(J: LLVMOrcLLJITRef, Result: *mut LLVMOrcExecutorAddress, Name: *const c_char) -> LLVMErrorRef;
    pub fn LLVMOrcJITDylibAddGenerator(JD: LLVMOrcJITDylibRef, DG: LLVMOrcDefinitionGeneratorRef);
    pub fn LLVMOrcCreateDynamicLibrarySearchGeneratorForProcess(Result: *mut LLVMOrcDefinitionGeneratorRef, GlobalPrefix: c_char, Filter: LLVMOrcSymbolPredicate, FilterCtx: *mut std::ffi::c_void) -> LLVMErrorRef;

    // Error handling
    pub fn LLVMGetErrorMessage(Err: LLVMErrorRef) -> *mut c_char;
    pub fn LLVMConsumeError(Err: LLVMErrorRef);
}

pub fn initialize_native_target() {
    unsafe {
        #[cfg(target_arch = "x86_64")]
        {
            LLVMInitializeX86TargetInfo();
            LLVMInitializeX86Target();
            LLVMInitializeX86TargetMC();
            LLVMInitializeX86AsmPrinter();
        }
        #[cfg(target_arch = "aarch64")]
        {
            LLVMInitializeAArch64TargetInfo();
            LLVMInitializeAArch64Target();
            LLVMInitializeAArch64TargetMC();
            LLVMInitializeAArch64AsmPrinter();
        }
    }
}
