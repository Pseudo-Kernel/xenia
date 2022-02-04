#if 0
//
// Generated by Microsoft (R) HLSL Shader Compiler 10.1
//
//
//
// Patch Constant signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// SV_TessFactor            0   x           0 QUADEDGE   float   x   
// SV_TessFactor            1   x           1 QUADEDGE   float   x   
// SV_TessFactor            2   x           2 QUADEDGE   float   x   
// SV_TessFactor            3   x           3 QUADEDGE   float   x   
// SV_InsideTessFactor      0   x           4  QUADINT   float   x   
// SV_InsideTessFactor      1   x           5  QUADINT   float   x   
//
//
// Input signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// XETESSFACTOR             0   x           0     NONE   float   x   
//
//
// Output signature:
//
// Name                 Index   Mask Register SysValue  Format   Used
// -------------------- ----- ------ -------- -------- ------- ------
// XEVERTEXID               0   x           0     NONE   float   x   
//
// Tessellation Domain   # of control points
// -------------------- --------------------
// Quadrilateral                           4
//
// Tessellation Output Primitive  Partitioning Type 
// ------------------------------ ------------------
// Clockwise Triangles            Even Fractional   
//
hs_5_1
hs_decls 
dcl_input_control_point_count 4
dcl_output_control_point_count 4
dcl_tessellator_domain domain_quad
dcl_tessellator_partitioning partitioning_fractional_even
dcl_tessellator_output_primitive output_triangle_cw
dcl_globalFlags refactoringAllowed
hs_control_point_phase 
dcl_output o0.x
mov o0.x, l(0)
ret 
hs_fork_phase 
dcl_hs_fork_phase_instance_count 4
dcl_input vForkInstanceID
dcl_input vicp[4][0].x
dcl_output_siv o0.x, finalQuadUeq0EdgeTessFactor
dcl_output_siv o1.x, finalQuadVeq0EdgeTessFactor
dcl_output_siv o2.x, finalQuadUeq1EdgeTessFactor
dcl_output_siv o3.x, finalQuadVeq1EdgeTessFactor
dcl_temps 1
dcl_indexrange o0.x 4
iadd r0.x, vForkInstanceID.x, l(3)
and r0.x, r0.x, l(3)
mov r0.y, vForkInstanceID.x
mov o[r0.y + 0].x, vicp[r0.x + 0][0].x
ret 
hs_fork_phase 
dcl_hs_fork_phase_instance_count 2
dcl_input vForkInstanceID
dcl_input vicp[4][0].x
dcl_output_siv o4.x, finalQuadUInsideTessFactor
dcl_output_siv o5.x, finalQuadVInsideTessFactor
dcl_temps 1
dcl_indexrange o4.x 2
ult r0.x, vForkInstanceID.x, l(1)
movc r0.x, r0.x, l(0), l(3)
ineg r0.y, vForkInstanceID.x
min r0.x, vicp[r0.y + 2][0].x, vicp[r0.x + 0][0].x
mov r0.y, vForkInstanceID.x
mov o[r0.y + 4].x, r0.x
ret 
// Approximately 14 instruction slots used
#endif

const BYTE adaptive_quad_hs[] =
{
     68,  88,  66,  67,  91, 114, 
    196,  93,  35, 195, 210,  49, 
    154,  70, 168, 244,   8, 132, 
     25, 246,   1,   0,   0,   0, 
    176,   4,   0,   0,   6,   0, 
      0,   0,  56,   0,   0,   0, 
    164,   0,   0,   0, 220,   0, 
      0,   0,  16,   1,   0,   0, 
    212,   1,   0,   0,  20,   4, 
      0,   0,  82,  68,  69,  70, 
    100,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,  60,   0, 
      0,   0,   1,   5,  83,  72, 
      0,   5,   0,   0,  60,   0, 
      0,   0,  19,  19,  68,  37, 
     60,   0,   0,   0,  24,   0, 
      0,   0,  40,   0,   0,   0, 
     40,   0,   0,   0,  36,   0, 
      0,   0,  12,   0,   0,   0, 
      0,   0,   0,   0,  77, 105, 
     99, 114, 111, 115, 111, 102, 
    116,  32,  40,  82,  41,  32, 
     72,  76,  83,  76,  32,  83, 
    104,  97, 100, 101, 114,  32, 
     67, 111, 109, 112, 105, 108, 
    101, 114,  32,  49,  48,  46, 
     49,   0,  73,  83,  71,  78, 
     48,   0,   0,   0,   1,   0, 
      0,   0,   8,   0,   0,   0, 
     32,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      3,   0,   0,   0,   0,   0, 
      0,   0,   1,   1,   0,   0, 
     88,  69,  84,  69,  83,  83, 
     70,  65,  67,  84,  79,  82, 
      0, 171, 171, 171,  79,  83, 
     71,  78,  44,   0,   0,   0, 
      1,   0,   0,   0,   8,   0, 
      0,   0,  32,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   3,   0,   0,   0, 
      0,   0,   0,   0,   1,  14, 
      0,   0,  88,  69,  86,  69, 
     82,  84,  69,  88,  73,  68, 
      0, 171,  80,  67,  83,  71, 
    188,   0,   0,   0,   6,   0, 
      0,   0,   8,   0,   0,   0, 
    152,   0,   0,   0,   0,   0, 
      0,   0,  11,   0,   0,   0, 
      3,   0,   0,   0,   0,   0, 
      0,   0,   1,  14,   0,   0, 
    152,   0,   0,   0,   1,   0, 
      0,   0,  11,   0,   0,   0, 
      3,   0,   0,   0,   1,   0, 
      0,   0,   1,  14,   0,   0, 
    152,   0,   0,   0,   2,   0, 
      0,   0,  11,   0,   0,   0, 
      3,   0,   0,   0,   2,   0, 
      0,   0,   1,  14,   0,   0, 
    152,   0,   0,   0,   3,   0, 
      0,   0,  11,   0,   0,   0, 
      3,   0,   0,   0,   3,   0, 
      0,   0,   1,  14,   0,   0, 
    166,   0,   0,   0,   0,   0, 
      0,   0,  12,   0,   0,   0, 
      3,   0,   0,   0,   4,   0, 
      0,   0,   1,  14,   0,   0, 
    166,   0,   0,   0,   1,   0, 
      0,   0,  12,   0,   0,   0, 
      3,   0,   0,   0,   5,   0, 
      0,   0,   1,  14,   0,   0, 
     83,  86,  95,  84, 101, 115, 
    115,  70,  97,  99, 116, 111, 
    114,   0,  83,  86,  95,  73, 
    110, 115, 105, 100, 101,  84, 
    101, 115, 115,  70,  97,  99, 
    116, 111, 114,   0, 171, 171, 
     83,  72,  69,  88,  56,   2, 
      0,   0,  81,   0,   3,   0, 
    142,   0,   0,   0, 113,   0, 
      0,   1, 147,  32,   0,   1, 
    148,  32,   0,   1, 149,  24, 
      0,   1, 150,  32,   0,   1, 
    151,  24,   0,   1, 106,   8, 
      0,   1, 114,   0,   0,   1, 
    101,   0,   0,   3,  18,  32, 
     16,   0,   0,   0,   0,   0, 
     54,   0,   0,   5,  18,  32, 
     16,   0,   0,   0,   0,   0, 
      1,  64,   0,   0,   0,   0, 
      0,   0,  62,   0,   0,   1, 
    115,   0,   0,   1, 153,   0, 
      0,   2,   4,   0,   0,   0, 
     95,   0,   0,   2,   0, 112, 
      1,   0,  95,   0,   0,   4, 
     18, 144,  33,   0,   4,   0, 
      0,   0,   0,   0,   0,   0, 
    103,   0,   0,   4,  18,  32, 
     16,   0,   0,   0,   0,   0, 
     11,   0,   0,   0, 103,   0, 
      0,   4,  18,  32,  16,   0, 
      1,   0,   0,   0,  12,   0, 
      0,   0, 103,   0,   0,   4, 
     18,  32,  16,   0,   2,   0, 
      0,   0,  13,   0,   0,   0, 
    103,   0,   0,   4,  18,  32, 
     16,   0,   3,   0,   0,   0, 
     14,   0,   0,   0, 104,   0, 
      0,   2,   1,   0,   0,   0, 
     91,   0,   0,   4,  18,  32, 
     16,   0,   0,   0,   0,   0, 
      4,   0,   0,   0,  30,   0, 
      0,   6,  18,   0,  16,   0, 
      0,   0,   0,   0,  10, 112, 
      1,   0,   1,  64,   0,   0, 
      3,   0,   0,   0,   1,   0, 
      0,   7,  18,   0,  16,   0, 
      0,   0,   0,   0,  10,   0, 
     16,   0,   0,   0,   0,   0, 
      1,  64,   0,   0,   3,   0, 
      0,   0,  54,   0,   0,   4, 
     34,   0,  16,   0,   0,   0, 
      0,   0,  10, 112,   1,   0, 
     54,   0,   0,   8,  18,  32, 
    144,   0,  26,   0,  16,   0, 
      0,   0,   0,   0,  10, 144, 
    161,   0,  10,   0,  16,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,  62,   0,   0,   1, 
    115,   0,   0,   1, 153,   0, 
      0,   2,   2,   0,   0,   0, 
     95,   0,   0,   2,   0, 112, 
      1,   0,  95,   0,   0,   4, 
     18, 144,  33,   0,   4,   0, 
      0,   0,   0,   0,   0,   0, 
    103,   0,   0,   4,  18,  32, 
     16,   0,   4,   0,   0,   0, 
     15,   0,   0,   0, 103,   0, 
      0,   4,  18,  32,  16,   0, 
      5,   0,   0,   0,  16,   0, 
      0,   0, 104,   0,   0,   2, 
      1,   0,   0,   0,  91,   0, 
      0,   4,  18,  32,  16,   0, 
      4,   0,   0,   0,   2,   0, 
      0,   0,  79,   0,   0,   6, 
     18,   0,  16,   0,   0,   0, 
      0,   0,  10, 112,   1,   0, 
      1,  64,   0,   0,   1,   0, 
      0,   0,  55,   0,   0,   9, 
     18,   0,  16,   0,   0,   0, 
      0,   0,  10,   0,  16,   0, 
      0,   0,   0,   0,   1,  64, 
      0,   0,   0,   0,   0,   0, 
      1,  64,   0,   0,   3,   0, 
      0,   0,  40,   0,   0,   4, 
     34,   0,  16,   0,   0,   0, 
      0,   0,  10, 112,   1,   0, 
     51,   0,   0,  12,  18,   0, 
     16,   0,   0,   0,   0,   0, 
     10, 144, 225,   0,   2,   0, 
      0,   0,  26,   0,  16,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,  10, 144, 161,   0, 
     10,   0,  16,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
     54,   0,   0,   4,  34,   0, 
     16,   0,   0,   0,   0,   0, 
     10, 112,   1,   0,  54,   0, 
      0,   7,  18,  32, 208,   0, 
      4,   0,   0,   0,  26,   0, 
     16,   0,   0,   0,   0,   0, 
     10,   0,  16,   0,   0,   0, 
      0,   0,  62,   0,   0,   1, 
     83,  84,  65,  84, 148,   0, 
      0,   0,  14,   0,   0,   0, 
      1,   0,   0,   0,   0,   0, 
      0,   0,   6,   0,   0,   0, 
      1,   0,   0,   0,   2,   0, 
      0,   0,   2,   0,   0,   0, 
      3,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      5,   0,   0,   0,   1,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,  11,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   4,   0,   0,   0, 
      3,   0,   0,   0,   4,   0, 
      0,   0,   3,   0,   0,   0, 
      0,   0,   0,   0,   0,   0, 
      0,   0,   0,   0,   0,   0
};
