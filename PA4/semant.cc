

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include "semant.h"
#include "utilities.h"
#include <vector>
#include <set>



extern int semant_debug;
extern char *curr_filename;

//////////////////////////////////////////////////////////////////////
//
// Symbols
//
// For convenience, a large number of symbols are predefined here.
// These symbols include the primitive type and method names, as well
// as fixed names used by the runtime system.
//
//////////////////////////////////////////////////////////////////////
static Symbol 
    arg,
    arg2,
    Bool,
    concat,
    cool_abort,
    copy,
    Int,
    in_int,
    in_string,
    IO,
    length,
    Main,
    main_meth,
    No_class,
    No_type,
    Object,
    out_int,
    out_string,
    prim_slot,
    self,
    SELF_TYPE,
    Str,
    str_field,
    substr,
    type_name,
    val;
//
// Initializing the predefined symbols.
//
static void initialize_constants(void)
{
    arg         = idtable.add_string("arg");
    arg2        = idtable.add_string("arg2");
    Bool        = idtable.add_string("Bool");
    concat      = idtable.add_string("concat");
    cool_abort  = idtable.add_string("abort");
    copy        = idtable.add_string("copy");
    Int         = idtable.add_string("Int");
    in_int      = idtable.add_string("in_int");
    in_string   = idtable.add_string("in_string");
    IO          = idtable.add_string("IO");
    length      = idtable.add_string("length");
    Main        = idtable.add_string("Main");
    main_meth   = idtable.add_string("main");
    //   _no_class is a symbol that can't be the name of any 
    //   user-defined class.
    No_class    = idtable.add_string("_no_class");
    No_type     = idtable.add_string("_no_type");
    Object      = idtable.add_string("Object");
    out_int     = idtable.add_string("out_int");
    out_string  = idtable.add_string("out_string");
    prim_slot   = idtable.add_string("_prim_slot");
    self        = idtable.add_string("self");
    SELF_TYPE   = idtable.add_string("SELF_TYPE");
    Str         = idtable.add_string("String");
    str_field   = idtable.add_string("_str_field");
    substr      = idtable.add_string("substr");
    type_name   = idtable.add_string("type_name");
    val         = idtable.add_string("_val");
}

static void add_class_attrs_to_env(Class_ c, SymbolTable<Symbol, Symbol>* env) {
    Features fs = c->get_features();
    for (int i = fs->first(); fs->more(i); i = fs->next(i)) {
        Feature f = fs->nth(i);
        attr_class* a = dynamic_cast<attr_class*>(f);
        if (a) env->addid(a->get_name(), new Symbol(a->get_type_decl()));
    }
}

 // helper: normalize SELF_TYPE to current_class for LUB/conforms computations
 static inline Symbol normalize_self_type(Symbol t, Symbol current_class) {
     return (t == SELF_TYPE) ? current_class : t;
 }


ClassTable::ClassTable(Classes classes)
    : classes(classes), semant_errors(0), error_stream(cerr) {

    install_basic_classes();
    // 先不做完整继承图检查，后面再补
}


void ClassTable::install_basic_classes() {

    // The tree package uses these globals to annotate the classes built below.
   // curr_lineno  = 0;
    Symbol filename = stringtable.add_string("<basic class>");
    
    // The following demonstrates how to create dummy parse trees to
    // refer to basic Cool classes.  There's no need for method
    // bodies -- these are already built into the runtime system.
    
    // IMPORTANT: The results of the following expressions are
    // stored in local variables.  You will want to do something
    // with those variables at the end of this method to make this
    // code meaningful.

    // 
    // The Object class has no parent class. Its methods are
    //        abort() : Object    aborts the program
    //        type_name() : Str   returns a string representation of class name
    //        copy() : SELF_TYPE  returns a copy of the object
    //
    // There is no need for method bodies in the basic classes---these
    // are already built in to the runtime system.

    Class_ Object_class =
	class_(Object, 
	       No_class,
	       append_Features(
			       append_Features(
					       single_Features(method(cool_abort, nil_Formals(), Object, no_expr())),
					       single_Features(method(type_name, nil_Formals(), Str, no_expr()))),
			       single_Features(method(copy, nil_Formals(), SELF_TYPE, no_expr()))),
	       filename);

    // 
    // The IO class inherits from Object. Its methods are
    //        out_string(Str) : SELF_TYPE       writes a string to the output
    //        out_int(Int) : SELF_TYPE            "    an int    "  "     "
    //        in_string() : Str                 reads a string from the input
    //        in_int() : Int                      "   an int     "  "     "
    //
    Class_ IO_class = 
	class_(IO, 
	       Object,
	       append_Features(
			       append_Features(
					       append_Features(
							       single_Features(method(out_string, single_Formals(formal(arg, Str)),
										      SELF_TYPE, no_expr())),
							       single_Features(method(out_int, single_Formals(formal(arg, Int)),
										      SELF_TYPE, no_expr()))),
					       single_Features(method(in_string, nil_Formals(), Str, no_expr()))),
			       single_Features(method(in_int, nil_Formals(), Int, no_expr()))),
	       filename);  

    //
    // The Int class has no methods and only a single attribute, the
    // "val" for the integer. 
    //
    Class_ Int_class =
	class_(Int, 
	       Object,
	       single_Features(attr(val, prim_slot, no_expr())),
	       filename);

    //
    // Bool also has only the "val" slot.
    //
    Class_ Bool_class =
	class_(Bool, Object, single_Features(attr(val, prim_slot, no_expr())),filename);

    //
    // The class Str has a number of slots and operations:
    //       val                                  the length of the string
    //       str_field                            the string itself
    //       length() : Int                       returns length of the string
    //       concat(arg: Str) : Str               performs string concatenation
    //       substr(arg: Int, arg2: Int): Str     substring selection
    //       
    Class_ Str_class =
	class_(Str, 
	       Object,
	       append_Features(
			       append_Features(
					       append_Features(
							       append_Features(
									       single_Features(attr(val, Int, no_expr())),
									       single_Features(attr(str_field, prim_slot, no_expr()))),
							       single_Features(method(length, nil_Formals(), Int, no_expr()))),
					       single_Features(method(concat, 
								      single_Formals(formal(arg, Str)),
								      Str, 
								      no_expr()))),
			       single_Features(method(substr, 
						      append_Formals(single_Formals(formal(arg, Int)), 
								     single_Formals(formal(arg2, Int))),
						      Str, 
						      no_expr()))),
	       filename);
        

// 关键：把基础类加入 classes 列表，否则 lookup_method / parent_of / conforms 都会找不到 IO/Object
Classes basic = nil_Classes();
basic = append_Classes(basic, single_Classes(Object_class));
basic = append_Classes(basic, single_Classes(IO_class));
basic = append_Classes(basic, single_Classes(Int_class));
basic = append_Classes(basic, single_Classes(Bool_class));
basic = append_Classes(basic, single_Classes(Str_class));

// 把基础类放在用户类前面（不要覆盖掉已有的用户类）
this->classes = append_Classes(basic, this->classes);


}

////////////////////////////////////////////////////////////////////
//
// semant_error is an overloaded function for reporting errors
// during semantic analysis.  There are three versions:
//
//    ostream& ClassTable::semant_error()                
//
//    ostream& ClassTable::semant_error(Class_ c)
//       print line number and filename for `c'
//
//    ostream& ClassTable::semant_error(Symbol filename, tree_node *t)  
//       print a line number and filename
//
///////////////////////////////////////////////////////////////////

ostream& ClassTable::semant_error(Class_ c)
{                                                             
    return semant_error(c->get_filename(),c);
}    

ostream& ClassTable::semant_error(Symbol filename, tree_node *t)
{
    error_stream << filename << ":" << t->get_line_number() << ": ";
    return semant_error();
}

ostream& ClassTable::semant_error()                  
{                                                 
    semant_errors++;                            
    return error_stream;
} 

method_class* ClassTable::lookup_method(Symbol class_name, Symbol method_name) {
    // 沿继承链向上找：class_name -> parent -> parent -> ...
    Symbol cur = class_name;

    while (cur != No_class) {
        // 在 cur 这个类自身找
        for (int i = classes->first(); classes->more(i); i = classes->next(i)) {
            Class_ c = classes->nth(i);
            if (c->get_name() != cur) continue;

            Features fs = c->get_features();
            for (int j = fs->first(); fs->more(j); j = fs->next(j)) {
                Feature f = fs->nth(j);
                method_class* m = dynamic_cast<method_class*>(f);
                if (m && m->get_name() == method_name) return m;
            }
            break; // 找到类了但没找到方法，跳出去去父类
        }

        cur = parent_of(cur);
    }

    return nullptr;
}



/*   This is the entry point to the semantic checker.

     Your checker should do the following two things:

     1) Check that the program is semantically correct
     2) Decorate the abstract syntax tree with type information
        by setting the `type' field in each Expression node.
        (see `tree.h')

     You are free to first do 1), make sure you catch all semantic
     errors. Part 2) can be done in a second stage, when you want
     to build mycoolc.
 */
void program_class::semant()
{
    initialize_constants();

    ClassTable *classtable = new ClassTable(classes);

    if (classtable->errors()) {
        cerr << "Compilation halted due to static semantic errors." << endl;
        exit(1);
    }

    // ====== 新增：类型检查 & 标注 ======
    for (int i = classes->first(); classes->more(i); i = classes->next(i)) {
        Class_ c = classes->nth(i);
        class__class* cc = dynamic_cast<class__class*>(c);
        if (!cc) continue;

        Symbol current_class = c->get_name();
        Features fs = c->get_features();

        // ✅ 每个 class 只建一次 obj_env
        SymbolTable<Symbol, Symbol> obj_env;
        obj_env.enterscope();

        // self : SELF_TYPE
        obj_env.addid(self, new Symbol(SELF_TYPE));

        // 属性加入 env（含继承的）
        add_class_attrs_to_env(c, &obj_env);

        // ✅ 第3步：在这里跑“本类 attr initializer”的 typecheck（只跑一次）
       for (int jj = fs->first(); fs->more(jj); jj = fs->next(jj)) {
            Feature f2 = fs->nth(jj);
            attr_class* a = dynamic_cast<attr_class*>(f2);
            if (!a) continue;

            // 取 init 表达式（如果没有就跳过）
            Expression init = a->get_init();   // 注意：你的接口可能叫 get_init()
            if (init) {
                Symbol t_init = init->tc(classtable, current_class, &obj_env);
                // 如果你要检查 conforms，就再加 conforms 检查
            }
        }

        // 再跑每个 method
        for (int j = fs->first(); fs->more(j); j = fs->next(j)) {
            Feature f = fs->nth(j);
            method_class* m = dynamic_cast<method_class*>(f);
            if (!m) continue;

            obj_env.enterscope(); // ✅ 方法作用域

            // 形参加入 env（x:Int, y:Bool）
            Formals formals = m->get_formals();
            for (int k = formals->first(); formals->more(k); k = formals->next(k)) {
                Formal fo = formals->nth(k);
                formal_class* ff = dynamic_cast<formal_class*>(fo);
                if (ff) {
                    obj_env.addid(ff->get_name(), new Symbol(ff->get_type_decl()));
                }
            }

            // 方法体 typecheck
            Symbol inferred_ret = m->get_expr()->tc(classtable, current_class, &obj_env);

            // ====== Step2: 检查“推断返回类型” conforms “声明返回类型” ======
            Symbol declared_ret = m->get_return_type();

            // 比较时把 SELF_TYPE 当作 current_class
            Symbol inferred_cmp = (inferred_ret == SELF_TYPE) ? current_class : inferred_ret;
            Symbol declared_cmp = (declared_ret == SELF_TYPE) ? current_class : declared_ret;

            if (!classtable->conforms(inferred_cmp, declared_cmp)) {
                Symbol filename = classtable->filename_of(current_class);
                classtable->semant_error(filename, m)
                    << "Inferred return type " << inferred_ret
                    << " of method " << m->get_name()
                    << " does not conform to declared return type " << declared_ret
                    << "." << endl;
            }


            obj_env.exitscope(); // ✅ 退出方法作用域
        }

        obj_env.exitscope(); // ✅ 退出 class 作用域

        if (classtable->errors()) {
            cerr << "Compilation halted due to static semantic errors." << endl;
            exit(1);
        }
    }
}


bool ClassTable::conforms(Symbol child, Symbol parent) {
    if (child == parent) return true;
    if (child == No_type) return true;   // No_type 视为可赋值给任何类型（常见约定）
    if (parent == Object) return true;

    Symbol cur = child;
    while (cur != No_class) {
        if (cur == parent) return true;
        cur = parent_of(cur);
    }
    return false;
}

Symbol ClassTable::lub(Symbol a, Symbol b) {
    if (a == No_type) return b;
    if (b == No_type) return a;

    // 注意：lub 本身不应该把 SELF_TYPE 直接变 Object
    // SELF_TYPE 的处理应该在调用 lub 之前先“规范化”为 current_class

    // 记录 a 的所有祖先
    std::set<Symbol> ancestors;
    Symbol cur = a;
    while (cur != No_class) {
        ancestors.insert(cur);
        cur = parent_of(cur);
    }

    // 从 b 向上找第一个在 ancestors 里的
    cur = b;
    while (cur != No_class) {
        if (ancestors.count(cur)) return cur;
        cur = parent_of(cur);
    }

    return Object; // 保底
}


Symbol ClassTable::parent_of(Symbol class_name) {
    for (int i = classes->first(); classes->more(i); i = classes->next(i)) {
        Class_ c = classes->nth(i);
        if (c->get_name() == class_name) return c->get_parent();
    }
    return No_class; // 找不到就返回 No_class
}

Symbol ClassTable::filename_of(Symbol class_name) {
    for (int i = classes->first(); classes->more(i); i = classes->next(i)) {
        Class_ c = classes->nth(i);
        if (c->get_name() == class_name) return c->get_filename();
    }
    return stringtable.add_string("<unknown>");
}

Symbol int_const_class::tc(ClassTable* ct, Symbol current_class,
                           SymbolTable<Symbol, Symbol>* obj_env) {
    set_type(Int);
    return Int;
}

Symbol bool_const_class::tc(ClassTable* ct, Symbol current_class,
                            SymbolTable<Symbol, Symbol>* obj_env) {
    set_type(Bool);
    return Bool;
}

Symbol object_class::tc(ClassTable* ct, Symbol current_class,
                        SymbolTable<Symbol, Symbol>* obj_env) {
    if (name == self) {          // self
        set_type(SELF_TYPE);
        return SELF_TYPE;
    }
    Symbol* t = obj_env->lookup(name);
    if (!t) {
        ct->semant_error() << "Undeclared identifier " << name << "." << endl;
        set_type(Object);
        return Object;
    }
    set_type(*t);
    return *t;
}

Symbol assign_class::tc(ClassTable* ct, Symbol current_class,
                        SymbolTable<Symbol, Symbol>* obj_env) {
    Symbol filename = ct->filename_of(current_class);

    // 1) self 不能赋值
    if (name == self) {
        ct->semant_error(filename, this)
          << "Cannot assign to 'self'." << endl;
        expr->tc(ct, current_class, obj_env);
        set_type(Object);
        return Object;
    }

    // 2) 右边先 tc
    Symbol t1 = expr->tc(ct, current_class, obj_env);

    // 3) 查左边变量类型
    Symbol t0 = No_type;
    Symbol* found = obj_env->lookup(name);   // DAT*，也就是 Symbol*
    if (!found) {
        ct->semant_error(filename, this)
          << "Assignment to undeclared variable " << name << "." << endl;
        set_type(t1);   // 很多实现这里 set_type(Object) 或 t1，都可以；官方通常不会看这里因为会 halt
        return t1;
    }
    t0 = *found;

    // 4) conforms
    // COOL 规则：在类 C 内，SELF_TYPE 在比较时按 C 处理（但表达式的 set_type 仍保留 SELF_TYPE）
    // 若左边声明类型是 SELF_TYPE，则只能赋 SELF_TYPE
    if (t0 == SELF_TYPE) {
        if (t1 != SELF_TYPE && t1 != No_type) {
            ct->semant_error(filename, this)
              << "Type " << t1
              << " of assigned expression does not conform to declared type "
              << t0 << " of identifier " << name << "." << endl;
        }
    } else {
        Symbol t1_cmp = (t1 == SELF_TYPE) ? current_class : t1;
        if (t1 != No_type && !ct->conforms(t1_cmp, t0)) {
            ct->semant_error(filename, this)
              << "Type " << t1
              << " of assigned expression does not conform to declared type "
              << t0 << " of identifier " << name << "." << endl;
        }
    }


    // 5) assign 表达式类型 = t1
    set_type(t1);
    return t1;
}


Symbol block_class::tc(ClassTable* ct, Symbol current_class,
                       SymbolTable<Symbol, Symbol>* obj_env) {
    Symbol last = Object;
    for (int i = body->first(); body->more(i); i = body->next(i)) {
        last = body->nth(i)->tc(ct, current_class, obj_env);
    }
    set_type(last);
    return last;
}

Symbol new__class::tc(ClassTable* ct, Symbol current_class,
                      SymbolTable<Symbol, Symbol>* obj_env) {
    if (type_name == SELF_TYPE) {
        set_type(SELF_TYPE);
        return SELF_TYPE;
    }
    set_type(type_name);
    return type_name;
}

Symbol no_expr_class::tc(ClassTable* ct, Symbol current_class,
                         SymbolTable<Symbol, Symbol>* obj_env) {
    set_type(No_type);
    return No_type;
}

Symbol string_const_class::tc(ClassTable* ct, Symbol current_class,
                              SymbolTable<Symbol, Symbol>* obj_env) {
    set_type(Str);
    return Str;
}

Symbol isvoid_class::tc(ClassTable* ct, Symbol current_class,
                        SymbolTable<Symbol, Symbol>* obj_env) {
    e1->tc(ct, current_class, obj_env);
    set_type(Bool);
    return Bool;
}
Symbol neg_class::tc(ClassTable* ct, Symbol current_class,
                     SymbolTable<Symbol, Symbol>* obj_env) {
    e1->tc(ct, current_class, obj_env);
    set_type(Int);
    return Int;
}

Symbol comp_class::tc(ClassTable* ct, Symbol current_class,
                      SymbolTable<Symbol, Symbol>* obj_env) {
    e1->tc(ct, current_class, obj_env);
    set_type(Bool);
    return Bool;
}

Symbol plus_class::tc(ClassTable* ct, Symbol current_class,
                      SymbolTable<Symbol, Symbol>* obj_env) {
    e1->tc(ct, current_class, obj_env);
    e2->tc(ct, current_class, obj_env);
    set_type(Int);
    return Int;
}
Symbol sub_class::tc(ClassTable* ct, Symbol current_class,
                     SymbolTable<Symbol, Symbol>* obj_env) {
    e1->tc(ct, current_class, obj_env);
    e2->tc(ct, current_class, obj_env);
    set_type(Int);
    return Int;
}
Symbol mul_class::tc(ClassTable* ct, Symbol current_class,
                     SymbolTable<Symbol, Symbol>* obj_env) {
    e1->tc(ct, current_class, obj_env);
    e2->tc(ct, current_class, obj_env);
    set_type(Int);
    return Int;
}
Symbol divide_class::tc(ClassTable* ct, Symbol current_class,
                        SymbolTable<Symbol, Symbol>* obj_env) {
    e1->tc(ct, current_class, obj_env);
    e2->tc(ct, current_class, obj_env);
    set_type(Int);
    return Int;
}

Symbol lt_class::tc(ClassTable* ct, Symbol current_class,
                    SymbolTable<Symbol, Symbol>* obj_env) {
    e1->tc(ct, current_class, obj_env);
    e2->tc(ct, current_class, obj_env);
    set_type(Bool);
    return Bool;
}
Symbol leq_class::tc(ClassTable* ct, Symbol current_class,
                     SymbolTable<Symbol, Symbol>* obj_env) {
    e1->tc(ct, current_class, obj_env);
    e2->tc(ct, current_class, obj_env);
    set_type(Bool);
    return Bool;
}
Symbol eq_class::tc(ClassTable* ct, Symbol current_class,
                    SymbolTable<Symbol, Symbol>* obj_env) {
    e1->tc(ct, current_class, obj_env);
    e2->tc(ct, current_class, obj_env);
    set_type(Bool);
    return Bool;
}

Symbol cond_class::tc(ClassTable* ct, Symbol current_class,
                      SymbolTable<Symbol, Symbol>* obj_env) {
    Symbol filename = ct->filename_of(current_class);

    Symbol p = pred->tc(ct, current_class, obj_env);
    if (p != Bool) {
        ct->semant_error(filename, this)
          << "Predicate of 'if' does not have type Bool." << endl;
    }

    Symbol t_then = then_exp->tc(ct, current_class, obj_env);
    Symbol t_else = else_exp->tc(ct, current_class, obj_env);

    // LUB 比较时：把 SELF_TYPE 先当作 current_class
    Symbol then_cmp = (t_then == SELF_TYPE) ? current_class : t_then;
    Symbol else_cmp = (t_else == SELF_TYPE) ? current_class : t_else;

    Symbol t = ct->lub(then_cmp, else_cmp);

    // 如果两边都是 SELF_TYPE，则结果保持 SELF_TYPE（官方通常就是这样输出）
    if (t_then == SELF_TYPE && t_else == SELF_TYPE) t = SELF_TYPE;

    set_type(t);
    return t;
}



Symbol loop_class::tc(ClassTable* ct, Symbol current_class,
                      SymbolTable<Symbol, Symbol>* obj_env) {
    pred->tc(ct, current_class, obj_env);
    body->tc(ct, current_class, obj_env);
    set_type(Object);
    return Object;
}

Symbol let_class::tc(ClassTable* ct, Symbol current_class,
                     SymbolTable<Symbol, Symbol>* obj_env){
    Symbol filename = ct->filename_of(current_class);

    // 1) init (如果有)
    Symbol init_type = No_type;
    if (init) init_type = init->tc(ct, current_class, obj_env);

    // 2) 类型检查：init_type conforms type_decl
    if (init && init_type != No_type && !ct->conforms(init_type, type_decl)) {
        ct->semant_error(filename, this)
          << "Inferred type " << init_type
          << " of initialization of " << identifier
          << " does not conform to identifier's declared type " << type_decl << "."
          << endl;
    }

    // 3) 新作用域 + 加变量
    obj_env->enterscope();
    obj_env->addid(identifier, new Symbol(type_decl));

    // 4) body
    Symbol body_type = body->tc(ct, current_class, obj_env);
    obj_env->exitscope();

    set_type(body_type);
    return body_type;
}


Symbol typcase_class::tc(ClassTable* ct, Symbol current_class,
                         SymbolTable<Symbol, Symbol>* obj_env) {
    Symbol filename = ct->filename_of(current_class);

    expr->tc(ct, current_class, obj_env);

    Symbol result_type = No_type;
    std::set<Symbol> seen_branch_types;

    bool all_selftype = true;  // 新增：记录是否所有 branch expr 的类型都是 SELF_TYPE

    for (int i = cases->first(); cases->more(i); i = cases->next(i)) {
        Case c = cases->nth(i);
        branch_class* br = dynamic_cast<branch_class*>(c);

        Symbol br_type = br->get_type_decl();
        Symbol br_name = br->get_name();

        if (seen_branch_types.count(br_type)) {
            ct->semant_error(filename, this)
              << "Duplicate branch " << br_type << " in case statement." << endl;
        } else {
            seen_branch_types.insert(br_type);
        }

        obj_env->enterscope();
        obj_env->addid(br_name, new Symbol(br_type));

        Symbol t = br->get_expr()->tc(ct, current_class, obj_env);

        obj_env->exitscope();

        if (t != SELF_TYPE) all_selftype = false;

        // 求 lub 时，SELF_TYPE 用 current_class 参与比较
        Symbol t_cmp = (t == SELF_TYPE) ? current_class : t;

        if (result_type == No_type) {
            result_type = t_cmp;
        } else {
            Symbol res_cmp = (result_type == SELF_TYPE) ? current_class : result_type;
            result_type = ct->lub(res_cmp, t_cmp);
        }
    }

    if (result_type == No_type) result_type = Object;

    // 关键：如果所有分支表达式的类型都是 SELF_TYPE，则 case 的结果类型也应该是 SELF_TYPE
    if (all_selftype) result_type = SELF_TYPE;

    set_type(result_type);
    return result_type;
}




Symbol dispatch_class::tc(ClassTable* ct, Symbol current_class,
                          SymbolTable<Symbol, Symbol>* obj_env) {

        // 0) 用 current_class 找到文件名，用于打印 bad.cl:line:
        Symbol filename = ct->filename_of(current_class);

        // 1) receiver 类型
        Symbol recv_t = expr->tc(ct, current_class, obj_env);

        // 2) 实参先递归 typecheck，并收集类型
        std::vector<Symbol> actual_types;
        for (int i = actual->first(); actual->more(i); i = actual->next(i)) {
            Symbol t = actual->nth(i)->tc(ct, current_class, obj_env);
            actual_types.push_back(t);
        }

        // 3) SELF_TYPE：在当前类查方法
        Symbol dispatch_class_name = recv_t;
        if (recv_t == SELF_TYPE) dispatch_class_name = current_class;

        method_class* m = ct->lookup_method(dispatch_class_name, name);
        if (!m) {
            ct->semant_error(filename, this)
            << "Dispatch to undefined method " << name << "." << endl;
            set_type(Object);
            return Object;
        }

        // 4) 参数个数检查
        Formals formals = m->get_formals();
        int n_formals = 0;
        for (int k = formals->first(); formals->more(k); k = formals->next(k)) n_formals++;

        if ((int)actual_types.size() != n_formals) {
            ct->semant_error(filename, this)
            << "Method " << name << " called with wrong number of arguments." << endl;
        } else {
        
        // 5) 参数类型检查（用 conforms + SELF_TYPE 处理）
        int ai = 0;
        Symbol recv_lookup = (recv_t == SELF_TYPE) ? current_class : recv_t;
        for (int k = formals->first(); formals->more(k); k = formals->next(k), ai++) {
            // Formal 在 PA4 里本来就是 formal_class*，不需要 dynamic_cast
            formal_class* ff = (formal_class*) formals->nth(k);
            if (ff == nullptr) continue; // 防御：避免崩溃

            Symbol declared = ff->get_type_decl();
            Symbol given    = actual_types[ai];

            // declared = SELF_TYPE（若你保留这条规则）：只有 SELF_TYPE 才能传给 SELF_TYPE
            if (declared == SELF_TYPE) {
                if (given != SELF_TYPE) {
                    ct->semant_error(filename, this)
                      << "In call of method " << name
                      << ", type " << given
                      << " of parameter " << ff->get_name()
                      << " does not conform to declared type " << declared
                      << "." << endl;
                }
            } else {
                // given = SELF_TYPE：按 receiver 的静态类型参与 conforms
                Symbol given_cmp = (given == SELF_TYPE) ? current_class : given;
                if (!ct->conforms(given_cmp, declared)) {
                    ct->semant_error(filename, this)
                      << "In call of method " << name
                      << ", type " << given
                      << " of parameter " << ff->get_name()
                      << " does not conform to declared type " << declared
                      << "." << endl;
                }
            }
        }
    }

        // 6) 返回类型推导（严格按 COOL）
        // receiver 的“静态类型”用于决定 SELF_TYPE 返回值
        Symbol receiver_static = recv_t;
        if (receiver_static == No_type) {
            // 有的 AST 会用 no_expr 表示 implicit dispatch（f(...) 相当于 self.f(...)）
            receiver_static = SELF_TYPE;
        }

        Symbol ret = m->get_return_type();
        if (ret == SELF_TYPE) {
            // 如果 receiver 静态类型是 SELF_TYPE，结果仍是 SELF_TYPE
            // 否则结果是 receiver 的静态类型（例如 IO）
            ret = receiver_static;
        }

        set_type(ret);
        return ret;

}
             



Symbol static_dispatch_class::tc(ClassTable* ct, Symbol current_class,
                                 SymbolTable<Symbol, Symbol>* obj_env) {
    Symbol filename = ct->filename_of(current_class);

    // 1) 先 typecheck receiver
    Symbol recv_t = expr->tc(ct, current_class, obj_env);
    Symbol recv_lookup = (recv_t == SELF_TYPE) ? current_class : recv_t;

    // 2) static dispatch: 必须保证 receiver 的类型 conforms to type_name
    Symbol target = type_name;                 // a@A.f(...) 里的 A
    Symbol target_cmp = (target == SELF_TYPE) ? current_class : target;

    if (!ct->conforms(recv_lookup, target_cmp)) {
        ct->semant_error(filename, this)
          << "Expression type " << recv_t
          << " does not conform to declared static dispatch type "
          << type_name << "." << endl;
        // 继续往下走，尽量把更多错误报出来
    }

    // 3) typecheck 实参并收集类型
    std::vector<Symbol> actual_types;
    for (int i = actual->first(); actual->more(i); i = actual->next(i)) {
        Symbol t = actual->nth(i)->tc(ct, current_class, obj_env);
        actual_types.push_back(t);
    }

    // 4) 在指定的静态类型 type_name 上查方法
    method_class* m = ct->lookup_method(target_cmp, name);
    if (!m) {
        ct->semant_error(filename, this)
          << "Dispatch to undefined method " << name << "." << endl;
        set_type(Object);
        return Object;
    }

    // 5) 参数个数与类型检查（用 conforms，而不是 ==）
    Formals formals = m->get_formals();
    int n_formals = 0;
    for (int k = formals->first(); formals->more(k); k = formals->next(k)) n_formals++;

    if ((int)actual_types.size() != n_formals) {
        ct->semant_error(filename, this)
          << "Method " << name << " called with wrong number of arguments." << endl;
    } else {
        int ai = 0;
        for (int k = formals->first(); formals->more(k); k = formals->next(k), ai++) {
            formal_class* ff = (formal_class*) formals->nth(k);
            Symbol declared = ff->get_type_decl();
            Symbol given = actual_types[ai];

            Symbol given_cmp = (given == SELF_TYPE) ? current_class : given;

            Symbol declared_cmp = (declared == SELF_TYPE) ? target_cmp : declared;

            if (!ct->conforms(given_cmp, declared_cmp)) {
                ct->semant_error(filename, this)
                  << "In call of method " << name
                  << ", type " << given
                  << " of parameter " << ff->get_name()
                  << " does not conform to declared type " << declared
                  << "." << endl;
            }
        }
    }

    // 6) 返回类型：如果方法声明返回 SELF_TYPE
    //    注意：static dispatch 返回 SELF_TYPE 时，结果应该是 “receiver 的静态类型”：
    //    a@A.g(): SELF_TYPE  ==> A
        Symbol ret = m->get_return_type();
        if (ret == SELF_TYPE) {
            // static dispatch：SELF_TYPE 按 @T 的 T 来解析
            ret = type_name;   // 这里的 type_name 就是 @T 的 T
        }
        set_type(ret);
        return ret;

}

