; ModuleID = '/home/r/Dokumente/Unisachen/cg-hiwi/imbatracer/stencil_int'
target datalayout = "e-p:64:64:64-i1:8:8-i8:8:8-i16:16:16-i32:32:32-i64:64:64-f32:32:32-f64:64:64-v64:64:64-v128:128:128-a0:0:64-s0:64:64-f80:128:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

define [0 x float]* @main_impala() {
main_impala_start:
  %current_ub = alloca [3 x i32]
  %current_lb = alloca [3 x i32]
  br label %main_impala

main_impala:                                      ; preds = %main_impala_start
  %0 = call i8* @thorin_malloc(i64 16777216)
  %1 = bitcast i8* %0 to [0 x float]*
  store [0 x float] undef, [0 x float]* %1
  %2 = call i8* @thorin_malloc(i64 16777216)
  %3 = bitcast i8* %2 to [0 x float]*
  store [0 x float] undef, [0 x float]* %3
  br label %range

range:                                            ; preds = %if_else4, %main_impala
  %a = phi i32 [ 0, %main_impala ], [ %6, %if_else4 ]
  %b = phi i32 [ 100, %main_impala ], [ %b, %if_else4 ]
  %4 = icmp slt i32 %a, %b
  br i1 %4, label %if_then, label %if_else

if_else:                                          ; preds = %range
  ret [0 x float]* %3

if_then:                                          ; preds = %range
  br label %range1

range1:                                           ; preds = %if_else9, %if_then
  %a2 = phi i32 [ 0, %if_then ], [ %12, %if_else9 ]
  %b3 = phi i32 [ 3, %if_then ], [ %b3, %if_else9 ]
  %5 = icmp slt i32 %a2, %b3
  br i1 %5, label %if_then5, label %if_else4

if_else4:                                         ; preds = %range1
  %6 = add i32 1, %a
  br label %range

if_then5:                                         ; preds = %range1
  store [3 x i32] [i32 0, i32 1, i32 2047], [3 x i32]* %current_lb
  %7 = getelementptr inbounds [3 x i32]* %current_lb, i64 0, i32 %a2
  %8 = load i32* %7
  store [3 x i32] [i32 1, i32 2047, i32 2048], [3 x i32]* %current_ub
  %9 = getelementptr inbounds [3 x i32]* %current_ub, i64 0, i32 %a2
  %10 = load i32* %9
  br label %range6

range6:                                           ; preds = %if_else14, %if_then5
  %a7 = phi i32 [ %8, %if_then5 ], [ %14, %if_else14 ]
  %b8 = phi i32 [ %10, %if_then5 ], [ %b8, %if_else14 ]
  %11 = icmp slt i32 %a7, %b8
  br i1 %11, label %if_then10, label %if_else9

if_else9:                                         ; preds = %range6
  %12 = add i32 1, %a2
  br label %range1

if_then10:                                        ; preds = %range6
  br label %range11

range11:                                          ; preds = %next22, %if_then10
  %a12 = phi i32 [ -1, %if_then10 ], [ %20, %next22 ]
  %b13 = phi i32 [ 2, %if_then10 ], [ %b13, %next22 ]
  %13 = icmp slt i32 %a12, %b13
  br i1 %13, label %if_then15, label %if_else14

if_else14:                                        ; preds = %range11
  %14 = add i32 1, %a7
  br label %range6

if_then15:                                        ; preds = %range11
  %15 = add i32 %a7, %a12
  %16 = icmp slt i32 %15, %8
  br i1 %16, label %if_then17, label %if_else16

if_else16:                                        ; preds = %if_then15
  br label %next

if_then17:                                        ; preds = %if_then15
  %17 = call i32 @bh_clamp_lower_160(i32 %15, i32 %8)
  br label %if_then18

if_then18:                                        ; preds = %if_then17
  br label %next

next:                                             ; preds = %if_then18, %if_else16
  %index = phi i32 [ %15, %if_else16 ], [ %17, %if_then18 ]
  %18 = icmp sle i32 %10, %index
  br i1 %18, label %if_then20, label %if_else19

if_else19:                                        ; preds = %next
  br label %next22

if_then20:                                        ; preds = %next
  %19 = call i32 @bh_clamp_upper_173(i32 %index, i32 %10)
  br label %if_then21

if_then21:                                        ; preds = %if_then20
  br label %next22

next22:                                           ; preds = %if_then21, %if_else19
  %index23 = phi i32 [ %index, %if_else19 ], [ %19, %if_then21 ]
  %20 = add i32 1, %a12
  br label %range11
}

define i32 @bh_clamp_lower_160(i32 %index_162, i32 %lower_163) {
bh_clamp_lower_160_start:
  br label %bh_clamp_lower

bh_clamp_lower:                                   ; preds = %bh_clamp_lower_160_start
  %0 = icmp slt i32 %index_162, %lower_163
  br i1 %0, label %if_then, label %if_else

if_else:                                          ; preds = %bh_clamp_lower
  br label %next

if_then:                                          ; preds = %bh_clamp_lower
  br label %next

next:                                             ; preds = %if_then, %if_else
  %1 = phi i32 [ %lower_163, %if_then ], [ %index_162, %if_else ]
  ret i32 %1
}

define i32 @bh_clamp_upper_173(i32 %index_175, i32 %upper_176) {
bh_clamp_upper_173_start:
  br label %bh_clamp_upper

bh_clamp_upper:                                   ; preds = %bh_clamp_upper_173_start
  %0 = icmp sle i32 %upper_176, %index_175
  br i1 %0, label %if_then, label %if_else

if_else:                                          ; preds = %bh_clamp_upper
  br label %next

if_then:                                          ; preds = %bh_clamp_upper
  %1 = add i32 -1, %upper_176
  br label %next

next:                                             ; preds = %if_then, %if_else
  %2 = phi i32 [ %1, %if_then ], [ %index_175, %if_else ]
  ret i32 %2
}

declare i8* @thorin_malloc(i64)
