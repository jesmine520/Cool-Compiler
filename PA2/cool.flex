/*
 *  The scanner definition for COOL.
 */

/*
 *  Stuff enclosed in %{ %} in the first section is copied verbatim to the
 *  output, so headers and global definitions are placed here to be visible
 * to the code in the file.  Don't remove anything that was here initially
 */
%{
#include <cool-parse.h>
#include <stringtab.h>
#include <utilities.h>

/* The compiler assumes these identifiers. */
#define yylval cool_yylval
#define yylex  cool_yylex

/* Max size of string constants */
#define MAX_STR_CONST 1025
#define YY_NO_UNPUT   /* keep g++ happy */

extern FILE *fin; /* we read from this file */

/* define YY_INPUT so we read from the FILE fin:
 * This change makes it possible to use this scanner in
 * the Cool compiler.
 */
#undef YY_INPUT
#define YY_INPUT(buf,result,max_size) \
	if ( (result = fread( (char*)buf, sizeof(char), max_size, fin)) < 0) \
		YY_FATAL_ERROR( "read() in flex scanner failed");

char string_buf[MAX_STR_CONST]; /* to assemble string constants */
char *string_buf_ptr;

extern int curr_lineno;
extern int verbose_flag;

extern YYSTYPE cool_yylval;

/*
 *  Add Your own definitions here
 */

%}

%option noyywrap
%x COMMENT
%x STRING

/*
 * Define names for regular expressions here.
 */

DARROW          =>
ASSIGN		<-
LE		<=
DIGIT		[0-9]
LOWER		[a-z]
UPPER		[A-Z]
LETTER		[a-zA-Z]
ALNUM		[a-zA-Z0-9]

 /*
  *	字符串缓冲与辅助
  */
%{


int str_overflow = 0;

static inline void str_reset(){
  string_buf_ptr = string_buf;
  str_overflow = 0;
}

static inline void str_put(char c){
  if (string_buf_ptr - string_buf >= MAX_STR_CONST - 1){
	str_overflow = 1;
  } else {
	*string_buf_ptr++ = c;
  }
}
%}

 /*
  *	嵌套注释层级计数
  */
%{
int comment_level = 0;
%}

%%

 /*
  *  Nested comments
  */


 /*
  *  嵌套注释层级计数
  */
{DARROW}		{ return (DARROW); }
{ASSIGN}		{ return ASSIGN; }
{LE}			{ return LE; }

 /*
  *	字符串：进入 STRING 状态
  */
\"	{ str_reset(); BEGIN(STRING); }

 /*
  *	STRING 状态内的处理
  */
<STRING>{
 /*
  *	1) 结束引号：收尾并返回 STR_CONST（若此前超长则报错）
  */ 
 \"	{
	  if (str_overflow) {
	    cool_yylval.error_msg = (char*)"String constant too long";
	    BEGIN(INITIAL);
	    return ERROR;
	  }
	  *string_buf_ptr = '\0';
	  cool_yylval.symbol = stringtable.add_string(string_buf);
	  BEGIN(INITIAL);
	  return STR_CONST;
	}
 /*
  *	 2) 原始换行：不允许，报“未闭合字符串”，并结束到 INITIAL
  */
  \n	{
	  curr_lineno++;
	  cool_yylval.error_msg = (char*)"Unterminated string constant";
	  BEGIN(INITIAL);
	  return ERROR;
	}
 /*
  *	3) 文件结束：报 EOF in string
  */
  <<EOF>> {
		cool_yylval.error_msg = (char*)"EOF in string constant";
		BEGIN(INITIAL);
		return ERROR;
	  }
 /*
  *	4) 合法转义：\n \t \b \f \" \\
  */ 
  \\n	  { str_put('\n'); }
  \\t     { str_put('\t'); }
  \\b     { str_put('\b'); }
  \\f     { str_put('\f'); }
  \\\"	  { str_put('\"'); }
  \\\\\\  { str_put('\\'); }

 /*
  *	5) \0（NUL）：字符串中禁止 NUL，直接报错并回到 INITIAL
  */
  \\0	{
	  cool_yylval.error_msg = (char*)"String contains null character";
	  BEGIN(INITIAL);
	  return ERROR;
	}

 /*
  *	6) 其他任意转义：把反斜杠后的字符原样放入（如 \x → x）
  */ 
  \\\.	{ str_put(yytext[1]); }

 /*
  *	7) 普通字符（除去引号、反斜杠、换行）
  */
  [^\"\\\n]+	{
		  for (char* p = yytext; *p; ++p) str_put(*p);
		}
}


 /*
  *	注释
  *	单行注释：-- 到行尾（不含换行）
  *	多行注释开始：支持嵌套
  *	在初始态遇到不匹配的 "*)"：报错
  */
"--"[^\n]*          { /* 吞掉到行尾，换行由 \n 规则处理 */ }
"(*"            { comment_level = 1; BEGIN(COMMENT); }
"*)"            {
  cool_yylval.error_msg = (char*)"Unmatched *)";
  return ERROR;
}


 /*
  *	COMMENT 状态内的规则
  */
<COMMENT>{
  "(*"		{ comment_level++; }
  "*)"		{ 
		  comment_level--;
		  if (comment_level == 0) BEGIN(INITIAL);
		}
  \n		{ curr_lineno++; }	// 统计行号 
  <<EOF>>	{
		  cool_yylval.error_msg = (char*)"EOF in comment";
		  BEGIN(INITIAL);
		  return ERROR;
		}
  .       { /* 吞掉注释中的其他任意字符（防止默认回显）*/ }
}


 /*
  *     忽略空白符与换行符
  */
[ \f\r\t\v]+		;
\n			{ curr_lineno++; }


 /*
  *	布尔常量
  */
[tT][rR][uU][eE] { cool_yylval.boolean = 1; return BOOL_CONST; }
[fF][aA][lL][sS][eE] { cool_yylval.boolean = 0; return BOOL_CONST; }




 /*
  *     关键字（大小写不敏感，放在标识符前）
  */
[cC][lL][aA][sS][sS]             { return CLASS; }
[iI][nN][hH][eE][rR][iI][tT][sS] { return INHERITS; }
[iI][fF]                         { return IF; }
[tT][hH][eE][nN]                 { return THEN; }
[eE][lL][sS][eE]                 { return ELSE; }
[fF][iI]                         { return FI; }
[wW][hH][iI][lL][eE]            { return WHILE; }
[lL][oO][oO][pP]                { return LOOP; }
[pP][oO][oO][lL]                { return POOL; }
[lL][eE][tT]                    { return LET; }
[iI][nN]                        { return IN; }
[cC][aA][sS][eE]                { return CASE; }
[oO][fF]                        { return OF; }
[eE][sS][aA][cC]                { return ESAC; }
[nN][eE][wW]                    { return NEW; }
[iI][sS][vV][oO][iI][dD]        { return ISVOID; }
[nN][oO][tT]                    { return NOT; }

 /*
  *	整数常量
  */
[0-9]+ {
  cool_yylval.symbol = stringtable.add_string(yytext);
  return INT_CONST;
}

 /*
  *	* 标识符规则
  * TYPEID：大写字母开头
  * OBJECTID：小写字母开头
  */
{UPPER}({UPPER}|{LOWER}|{DIGIT}|_)* {
  cool_yylval.symbol = stringtable.add_string(yytext);
  return TYPEID;
}

{LOWER}({UPPER}|{LOWER}|{DIGIT}|_)* {
  cool_yylval.symbol = stringtable.add_string(yytext);
  return OBJECTID;
}
 
 /*
  *	符号与分隔符
  */ 
"{"	{ return '{'; }
"}"     { return '}'; }
"("     { return '('; }
")"     { return ')'; }
":"     { return ':'; }
";"     { return ';'; }
","     { return ','; }
"<"     { return '<'; }
"="     { return '='; }
"+"     { return '+'; }
"-"     { return '-'; }
"*"     { return '*'; }
"/"     { return '/'; }
"-"	{ return '-'; }
"."	{ return '.'; }
"@"	{ return '@'; }
"~"	{ return '~'; }



 /*
  *	兜底规则：未匹配到的任何字符都视为错误
  */
. {
  cool_yylval.error_msg = yytext;
  return ERROR;
}


 /*
  * Keywords are case-insensitive except for the values true and false,
  * which must begin with a lower-case letter.
  */


 /*
  *  String constants (C syntax)
  *  Escape sequence \c is accepted for all characters c. Except for 
  *  \n \t \b \f, the result is c.
  *
  */

%%
