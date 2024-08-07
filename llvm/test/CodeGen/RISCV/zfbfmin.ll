; NOTE: Assertions have been autogenerated by utils/update_llc_test_checks.py
; RUN: llc -mtriple=riscv32 -mattr=+d,+zfh,+zfbfmin -verify-machineinstrs \
; RUN:   -target-abi ilp32d < %s | FileCheck -check-prefix=CHECKIZFBFMIN %s
; RUN: llc -mtriple=riscv64 -mattr=+d,+zfh,+zfbfmin -verify-machineinstrs \
; RUN:   -target-abi lp64d < %s | FileCheck -check-prefix=CHECKIZFBFMIN %s

define bfloat @bitcast_bf16_i16(i16 %a) nounwind {
; CHECKIZFBFMIN-LABEL: bitcast_bf16_i16:
; CHECKIZFBFMIN:       # %bb.0:
; CHECKIZFBFMIN-NEXT:    fmv.h.x fa0, a0
; CHECKIZFBFMIN-NEXT:    ret
  %1 = bitcast i16 %a to bfloat
  ret bfloat %1
}

define i16 @bitcast_i16_bf16(bfloat %a) nounwind {
; CHECKIZFBFMIN-LABEL: bitcast_i16_bf16:
; CHECKIZFBFMIN:       # %bb.0:
; CHECKIZFBFMIN-NEXT:    fmv.x.h a0, fa0
; CHECKIZFBFMIN-NEXT:    ret
  %1 = bitcast bfloat %a to i16
  ret i16 %1
}

define bfloat @fcvt_bf16_s(float %a) nounwind {
; CHECKIZFBFMIN-LABEL: fcvt_bf16_s:
; CHECKIZFBFMIN:       # %bb.0:
; CHECKIZFBFMIN-NEXT:    fcvt.bf16.s fa0, fa0
; CHECKIZFBFMIN-NEXT:    ret
  %1 = fptrunc float %a to bfloat
  ret bfloat %1
}

define float @fcvt_s_bf16(bfloat %a) nounwind {
; CHECKIZFBFMIN-LABEL: fcvt_s_bf16:
; CHECKIZFBFMIN:       # %bb.0:
; CHECKIZFBFMIN-NEXT:    fcvt.s.bf16 fa0, fa0
; CHECKIZFBFMIN-NEXT:    ret
  %1 = fpext bfloat %a to float
  ret float %1
}

define bfloat @fcvt_bf16_d(double %a) nounwind {
; CHECKIZFBFMIN-LABEL: fcvt_bf16_d:
; CHECKIZFBFMIN:       # %bb.0:
; CHECKIZFBFMIN-NEXT:    fcvt.s.d fa5, fa0
; CHECKIZFBFMIN-NEXT:    fcvt.bf16.s fa0, fa5
; CHECKIZFBFMIN-NEXT:    ret
  %1 = fptrunc double %a to bfloat
  ret bfloat %1
}

define double @fcvt_d_bf16(bfloat %a) nounwind {
; CHECKIZFBFMIN-LABEL: fcvt_d_bf16:
; CHECKIZFBFMIN:       # %bb.0:
; CHECKIZFBFMIN-NEXT:    fcvt.s.bf16 fa5, fa0
; CHECKIZFBFMIN-NEXT:    fcvt.d.s fa0, fa5
; CHECKIZFBFMIN-NEXT:    ret
  %1 = fpext bfloat %a to double
  ret double %1
}

define bfloat @bfloat_load(ptr %a) nounwind {
; CHECKIZFBFMIN-LABEL: bfloat_load:
; CHECKIZFBFMIN:       # %bb.0:
; CHECKIZFBFMIN-NEXT:    flh fa0, 6(a0)
; CHECKIZFBFMIN-NEXT:    ret
  %1 = getelementptr bfloat, ptr %a, i32 3
  %2 = load bfloat, ptr %1
  ret bfloat %2
}

define bfloat @bfloat_imm() nounwind {
; CHECKIZFBFMIN-LABEL: bfloat_imm:
; CHECKIZFBFMIN:       # %bb.0:
; CHECKIZFBFMIN-NEXT:    lui a0, %hi(.LCPI7_0)
; CHECKIZFBFMIN-NEXT:    flh fa0, %lo(.LCPI7_0)(a0)
; CHECKIZFBFMIN-NEXT:    ret
  ret bfloat 3.0
}

define dso_local void @bfloat_store(ptr %a, bfloat %b) nounwind {
; CHECKIZFBFMIN-LABEL: bfloat_store:
; CHECKIZFBFMIN:       # %bb.0:
; CHECKIZFBFMIN-NEXT:    fsh fa0, 0(a0)
; CHECKIZFBFMIN-NEXT:    fsh fa0, 16(a0)
; CHECKIZFBFMIN-NEXT:    ret
  store bfloat %b, ptr %a
  %1 = getelementptr bfloat, ptr %a, i32 8
  store bfloat %b, ptr %1
  ret void
}
