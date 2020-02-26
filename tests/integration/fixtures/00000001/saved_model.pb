³

ª}
.
Identity

input"T
output"T"	
Ttype

NoOp
C
Placeholder
output"dtype"
dtypetype"
shapeshape:"serve*2.0.02v2.0.0-rc2-26-g64c3d382ca8ô
H
string_placeholderPlaceholder*
dtype0*
_output_shapes
:
I
float32_placeholderPlaceholder*
dtype0*
_output_shapes
:
G
int64_placeholderPlaceholder*
dtype0	*
_output_shapes
:
Q
string_predictIdentitystring_placeholder*
T0*
_output_shapes
:
S
float32_predictIdentityfloat32_placeholder*
T0*
_output_shapes
:
O
int64_predictIdentityint64_placeholder*
T0	*
_output_shapes
:

init_all_tablesNoOp"w"*
saved_model_main_op

init_all_tables*ª
inputsŸ
(
	int_input
int64_placeholder:0	
,
string_input
string_placeholder:0
,
float_input
float32_placeholder:0%

int_output
int64_predict:0	)
string_output
string_predict:0)
float_output
float32_predict:0tensorflow/serving/predict*³
serving_defaultŸ
,
float_input
float32_placeholder:0
(
	int_input
int64_placeholder:0	
,
string_input
string_placeholder:0%

int_output
int64_predict:0	)
string_output
string_predict:0)
float_output
float32_predict:0tensorflow/serving/predict