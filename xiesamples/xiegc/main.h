/*
Copyright 1996 by Syd Logan 

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its 
documentation for any purpose and without fee is hereby granted, 
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in 
supporting documentation, and that the name of Syd Logan not be
used in advertising or publicity pertaining to distribution of the
software without specific, written prior permission.  

SYD LOGAN DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
SYD LOGAN BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

*/

/*
** xgc
**
** xgc.h
**
** Initialization of structures, etc.
*/

/* The three columns in the XgcData arrays are:
**   name: the name of the toggle button
**   text: the corresponding text in the xgc syntax
**   code: the integer that the text corresponds to, for sending stuff
**         to X calls, etc.
*/

XgcData LogicalData[NUM_LOGICAL] = {
  {"clear",        "clear",        GXclear},
  {"and",          "and",          GXand},
  {"andReverse",   "andReverse",   GXandReverse},
  {"copy",         "copy",         GXcopy},
  {"andInverted",  "andInverted",  GXandInverted},
  {"noop",         "noop",         GXnoop},
  {"xor",          "xor",          GXxor},
  {"or",           "or",           GXor},
  {"nor",          "nor",          GXnor},
  {"equiv",        "equiv",        GXequiv},
  {"invert",       "invert",       GXinvert},
  {"orReverse",    "orReverse",    GXorReverse},
  {"copyInverted", "copyInverted", GXcopyInverted},
  {"orInverted",   "orInverted",   GXorInverted},
  {"nand",         "nand",         GXnand},
  {"set",          "set",          GXset}
}; 

/* The two rows in the XgcStuff structure are:
**   name of label, xgc syntax text, # of toggles, # of columns of toggles
**     (0 columns means 1 row, as many columns as necessary)
**   appropriate XgcData
*/

XgcStuff LogicalStuff = {
  {"Logical","logical",NUM_LOGICAL,3},
  LogicalData
};

XgcData TestData[NUM_TESTS] = {
  {"Arithmetic",     "Arithmetic",      ArithmeticTest},
  {"Compare",        "Compare",     CompareTest},
  {"Logical",        "Logical",      LogicalTest},
  {"Math",           "Math",   MathTest}
};

XgcStuff TestStuff = {
  {"Test","test",NUM_TESTS,2},
  TestData
};

XgcData ArithmeticData[NUM_ARITHMETIC] = {
  {"Add",      	"Add",       xieValAdd},
  {"Sub",  	"Sub",   xieValSub},
  {"SubRev",  	"SubRev",   xieValSubRev},
  {"Mul",  	"Mul",   xieValMul},
  {"Div",  	"Div",   xieValDiv},
  {"DivRev",  	"DivRev",   xieValDivRev},
  {"Min",  	"Min",   xieValMin},
  {"Max",  	"Max",   xieValMax},
  {"Gamma", 	"Gamma",  xieValGamma}
};

XgcStuff ArithmeticStuff = {
  {"Arithmetic","arithmetic",NUM_ARITHMETIC,3},
  ArithmeticData
};

XgcData MathData[NUM_MATH] = {
  {"Exp",    "Exp",     xieValExp},
  {"Ln",       "Ln",        xieValLn},
  {"Log2",      "Log2",       xieValLog2},
  {"Log10",      "Log10",       xieValLog10},
  {"Square",      "Square",       xieValSquare},
  {"Sqrt", "Sqrt",  xieValSqrt}
};

XgcStuff MathStuff = {
  {"Math","Math",NUM_MATH,3},
  MathData
};

XgcData CompareData[NUM_COMPARE] = {
  {"Greater", "Greater", xieValGT},
  {"Less", "Less", xieValLT},
  {"GreaterEqual", "GreaterEqual", xieValGE},
  {"LessEqual", "LessEqual", xieValLE},
  {"Equal", "Equal", xieValEQ},
  {"NotEqual", "NotEqual", xieValNE}
};

XgcStuff CompareStuff = {
  {"Compare","compare",NUM_COMPARE,2},
  CompareData
};

XgcData CompareCombineData[NUM_COMPARE_COMBINE] = {
  {"True", "True", True},
  {"False", "False", False}
};

XgcStuff CompareCombineStuff = {
  {"Combine","combine",NUM_COMPARE_COMBINE,0},
  CompareCombineData
};

XgcData TypeData[NUM_TYPE] = {
  {"Monadic", "Monadic", 0},
  {"Dyadic", "Dyadic", 1}
};

XgcStuff TypeStuff = {
  {"Inputs","inputs",NUM_TYPE,0},
  TypeData
};

/* Pointers to all the Xgcstuffs so we can run them through a loop */

static XgcStuff *Everything[7] = {
  &LogicalStuff,
  &ArithmeticStuff,
  &MathStuff,
  &CompareStuff,
  &CompareCombineStuff,
  &TypeStuff,
  &TestStuff
};
